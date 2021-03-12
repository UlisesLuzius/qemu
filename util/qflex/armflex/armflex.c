#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"
#include "hw/core/cpu.h"

#include "qflex/qflex.h"
#include "qflex/qflex-arch.h"

#include "qflex/armflex/armflex.h"
#include "qflex/armflex/aws/fpga.h"
#include "qflex/armflex/aws/fpga_interface.h"
#include "qflex/armflex/armflex-page-demander.h"

ArmflexConfig armflexConfig = { false, false };

static FPGAContext* c;
static ArmflexArchState state;

uint32_t req_pending_v = 0;
uint32_t req_pending[32][2] = {0};

uint32_t evict_pending_v = 0;
uint32_t evict_pending[32][2] = {0};

uint32_t cpu2pid_attached[32] = {0};
int cpu_count = 0;

static CPUState *get_pid2cpu(int pid) {
    for(int idx = 0; idx < 32; idx ++) {
        if(cpu2pid_attached[idx] == pid) {
            return qemu_get_cpu(idx);
        }
    }
    exit(1);
}

static void push_page_fault(uint64_t hvp, uint64_t ipt_bits) {
    uint64_t *synonyms = NULL;
    QEMUMissReply reply = {
        .type = sMissReply,
        .vpn = IPT_GET_VA(ipt_bits),
        .pid = IPT_GET_PID(ipt_bits),
        .permission = IPT_GET_PER(ipt_bits),
        .synonym_v = false
    };
    int ret = armflex_ipt_add_entry(hvp, ipt_bits, synonyms);
    if (ret == SYNONYM) {
        reply.synonym_v = true;
        reply.s_vpn = IPT_GET_VA(synonyms[0]);
        reply.s_pid = IPT_GET_PID(synonyms[0]);
	} else if (ret == PAGE) {
		// 2b. send 4K PAGE to FPGA
 		// armflex_push_resp(cpu, ipt_bits, PAGE); // TODO
		// armflex_push_page(cpu, hvp); // TODO
        pushPageToPageBuffer(c, (void*) hvp);
	}

	free(synonyms);
	sendMessageToFPGA(c, (void *) &reply, sizeof(reply));
}

static void push_raw(uint64_t hvp, uint64_t ipt_bits) {
    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if(!(req_pending_v & mask)) {
            req_pending_v |= mask;
            req_pending[i][0] = hvp;
            req_pending[i][1] = ipt_bits;
        }
    }
}

static bool has_raw(uint64_t hvp) {
    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if((evict_pending_v & mask) && hvp == evict_pending[i][0]) {
            return true;
        }
    }
    return false;
}

static void run_pending_raw(uint64_t hvp) {
    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if((req_pending_v & mask) && hvp == req_pending[i][0]) {
            req_pending_v &= ~mask;
            push_page_fault(req_pending[i][0], req_pending[i][1]);
        }
    }
}

static void handle_evict_notify(PageEvictNotification *message) {
    uint64_t ipt_bits = IPT_COMPRESS(message->vpn, message->pid, message->permission);
    CPUState *cpu = get_pid2cpu(message->pid);
	uint64_t hvp = gva_to_hva(cpu, message->vpn & PAGE_MASK, message->permission);
    assert(hvp != -1);
 
    if(message->modified) {
        int mask = 0;
        for(int i = 0; i < 32; i++) {
            mask = 1 << i;
            if(!(evict_pending_v & mask)) {
                evict_pending_v |= mask;
                evict_pending[i][0] = hvp;
                evict_pending[i][1] = ipt_bits;
                break;
            }
        }
    } else {
        armflex_ipt_evict(hvp, ipt_bits);
    }
}


static void handle_evict_writeback(PageEvictNotification* message) {
    uint64_t ipt_bits = IPT_COMPRESS(message->vpn, message->pid, message->permission);
    CPUState *cpu = get_pid2cpu(message->pid);
	uint64_t hvp = gva_to_hva(cpu, message->vpn, message->permission);
    assert(hvp != -1);

    fetchPageFromPageBuffer(c, (void *) hvp);
    armflex_ipt_evict(hvp, ipt_bits);
    run_pending_raw(hvp);

    int mask = 0;
    for(int i = 0; i < 32; i++) {
        mask = 1 << i;
        if(evict_pending_v & mask) {
            if(ipt_bits == evict_pending[i][1]) {
                evict_pending_v &= ~mask;
                assert(hvp == evict_pending[i][0]);
                break;
            }
        }
    }
}


static void handle_page_fault(PageFaultNotification *message) {
    // Check for RAW
    uint64_t ipt_bits = IPT_COMPRESS(message->vpn, message->pid, message->permission);
    CPUState *cpu = get_pid2cpu(message->pid);
	uint64_t hvp = gva_to_hva(cpu, message->vpn & PAGE_MASK, message->permission);
    if(hvp == -1) {
	    // Permission Violation
	    // armflex_push_resp(cpu, ipt_bits, FAULT); // TODO
        return;
    }

    if(has_raw(hvp)) {
        push_raw(hvp, ipt_bits);
    } else {
        push_page_fault(hvp, ipt_bits);
    }
}

static void run_requests(void) {
    uint8_t message[64];
    MessageType type;
    if(queryMessageFromFPGA(c, message)) {
        type = ((uint32_t*) message)[0];
        switch(type) {
            case sPageFaultNotify:
            handle_page_fault((PageFaultNotification *) message);
            break;
            case sEvictNotify:
            handle_evict_notify((PageEvictNotification *) message);
            break;
            case sEvictDone:
            handle_evict_writeback((PageEvictNotification *) message);
            break;
            default:
            exit(1);
            break;
        }
    }
}


static void run_transplant(CPUState *cpu, uint32_t thread) {
    transplant_getState(c, thread, (uint64_t *) &state, ARMFLEX_TOT_REGS);
    armflex_unpack_archstate(cpu, &state);
    qflex_cpu_step(cpu);
    armflex_pack_archstate(&state, cpu);
    transplant_pushState(c, thread, (uint64_t *) &state, ARMFLEX_TOT_REGS);
    transplant_start(c, thread);
}

static void run_transplants(void) {
    uint32_t pending = 0;
    uint32_t thread = 0;
    uint32_t mask = 1;
    CPUState *cpu = first_cpu;
    transplant_pending(c, &pending);
    if(pending) {
        while(cpu) {
            if(pending & mask) { 
                pending &= ~mask;
                run_transplant(cpu, thread);
            }
            mask<<=1;
            thread++;
            cpu = CPU_NEXT(cpu);
        }
    }
}

static void push_cpus(void) {
    CPUState *cpu = first_cpu;
    while(cpu) {
        cpu2pid_attached[cpu_count] = QFLEX_GET_ARCH(pid)(cpu);
        registerThreadWithProcess(c, cpu_count, cpu2pid_attached[cpu_count]);
        cpu_count++;
    }
}

int armflex_execution_flow(void) {
    initFPGAContext(c);
    push_cpus();
    while(1) {
       // Check for pending transplants
       run_transplants();
       // Check for pending requests
       run_requests();
    }
    releaseFPGAContext(c);
    cpu_count = 0;
}