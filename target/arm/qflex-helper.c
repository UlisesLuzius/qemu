#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/exec-all.h"

#include "qflex/qflex.h"
#include "qflex/qflex-profiling.h"
#include "qflex-helper.h"
#include "qflex/qflex-helper-a64.h"
#include "qflex/qflex-models.h"

#ifdef CONFIG_FA_QFLEX
#include "qflex/fa-qflex.h"
#endif /* CONFIG_FA_QFLEX */

/* TCG helper functions. (See exec/helper-proto.h  and target/arch/helper-a64.h)
 * This one expands prototypes for the helper functions.
 * They get executed in the TB
 * To use them: in translate.c or translate-a64.c
 * ex: HELPER(qflex_func)(arg1, arg2, ..., argn)
 * gen_helper_qflex_func(arg1, arg2, ..., argn)
 */

/**
 * @brief HELPER(qflex_mem_trace)
 * Helper gets executed before a LD/ST
 */
void HELPER(qflex_mem_trace)(CPUARMState* env, uint64_t addr, uint64_t type) {
	CPUState *cs = CPU(env_archcpu(env));
	qflex_log_mask(QFLEX_LOG_LDST, "[MEM]CPU%u:%lu:0x%016lx\n", cs->cpu_index, type, addr);

	if(qflex_mem_trace_gen_trace()) {
		uint64_t paddr = *((uint64_t *) vaddr_to_paddr(cs, addr));
		qflex_mem_trace_memaccess(addr, paddr, cs->cpu_index, type);
	}
#ifdef CONFIG_ARMFLEX
	if(armflex_is_running()) {
		if(type != MMU_INST_FETCH) {
			armflex_synchronize_page(addr, cs, type);
		}
	}
	if(armflex_gen_verification()) {
		if(type == MMU_INST_FETCH) {
			armflex_verification_gen_state();
		} else {
			armflex_verification_add_mem();
		}
	}
#endif
}

/**
 * @brief HELPER(qflex_executed_instruction)
 * location: location of the gen_helper_ in the transalation.
 *           EXEC_IN : Started executing a TB
 *           EXEC_OUT: Done executing a TB, NOTE: Branches don't trigger this helper.
 */
void HELPER(qflex_executed_instruction)(CPUARMState* env, uint64_t pc, int location) {
    CPUState *cs = CPU(env_archcpu(env));

    switch(location) {
        case QFLEX_EXEC_IN:
            if(unlikely(qflex_loglevel_mask(QFLEX_LOG_TB_EXEC)) && // To not print twice, Profile has priority
                    unlikely(!qflex_loglevel_mask(QFLEX_LOG_PROFILE_INST))) {
                FILE* logfile = qemu_log_lock();
                qemu_log("IN[%d]  :", cs->cpu_index);
                log_target_disas(cs, pc, 4);
                qemu_log_unlock(logfile);
            }
            qflex_update_inst_done(true);
            break;
        default: break;
    }
}

void HELPER(qflex_profile)(CPUARMState* env, uint64_t pc, int l1h, int l2h, int ldst) {
    if(!qflex_is_profiling()) return; // If not profiling, don't count statistics

    int el = (arm_current_el(env) == 0) ? 0 : 1;
    qflex_profile_stats.global_profiles_l1h[el][l1h]++;
    qflex_profile_stats.global_profiles_l2h[el][l2h]++;
    qflex_profile_stats.curr_el_profiles_l1h[l1h]++;
    qflex_profile_stats.curr_el_profiles_l2h[l2h]++;
    if(l1h == LDST) { qflex_profile_stats.global_ldst[ldst]++; }

    if(unlikely(qflex_loglevel_mask(QFLEX_LOG_PROFILE_INST))) {
        CPUState *cs = CPU(env_archcpu(env));
        const char* l1h_str = qflex_profile_get_string_l1h(l1h);
        const char* l2h_str = qflex_profile_get_string_l2h(l2h);

        char profile[256];
        snprintf(profile, 256, "PROFILE:[%02i:%02i]:%14s:%-18s | ", l1h, l2h, l1h_str, l2h_str);
        FILE * logfile = qemu_log_lock();
        qemu_log("%s", profile);
        log_target_disas(cs, pc, 4);
        qemu_log_unlock(logfile);
    }
}

/**
 * @brief HELPER(qflex_magic_insn)
 * In ARM, hint instruction (which is like a NOP) comes with an int with range 0-127
 * Big part of this range is defined as a normal NOP.
 * Too see which indexes are already used ref (curently 39-127 is free) :
 * https://developer.arm.com/docs/ddi0596/a/a64-base-instructions-alphabetic-order/hint-hint-instruction
 *
 * This function is called when a HINT n (90 < n < 127) TB is executed
 * nop_op: in HINT n, it's the selected n.
 *
 * To do more complex calls from QEMU guest to host
 * You can chain two nops
 *
 */
#include "qflex/qflex-qemu-calls.h"
static uint64_t prev_nop_op = 0;

static inline void qflex_mem_trace_cmds(uint64_t nop_op) {
    switch(nop_op) {
        case MEM_TRACE_START:   qflex_mem_trace_start(1 << 9); break;
        case MEM_TRACE_STOP:    qflex_mem_trace_stop();       break;
        case MEM_TRACE_RESULTS: qflex_mem_trace_log_direct(); break;
    }
}

static inline void qflex_cmds(uint64_t nop_op) {
    switch(nop_op) {
        default: break;
    }
}

void HELPER(qflex_magic_insn)(CPUARMState *env, uint64_t nop_op) {
    assert(nop_op >= 90);
    assert(nop_op <= 127);
    qflex_log_mask(QFLEX_LOG_MAGIC_INSN, "MAGIC_INST:%lu\n", nop_op);

    // CPUState *cs = CPU(env_archcpu(env));
 
    // Check first for chained nop_op
    switch(prev_nop_op) {

        case QFLEX_OP: 
            qflex_cmds(nop_op);
            prev_nop_op = 0;
            return; // HELPER EXIT

        case MEM_TRACE_OP: 
            qflex_mem_trace_cmds(nop_op);
            prev_nop_op = 0;
            return; // HELPER EXIT

        default: break;
    }

    // Get chained nop_op
    switch(nop_op) {
        case QFLEX_OP: prev_nop_op = QFLEX_OP; break;
        case MEM_TRACE_OP: prev_nop_op = MEM_TRACE_OP; break;
        default: prev_nop_op = 0; break;
    }
}

/**
 * @brief HELPER(qflex_exception_return)
 * This helper gets called after a ERET TB execution is done.
 * The env passed as argument has already changed EL and jumped to the ELR.
 * For the moment not needed.
 */
void HELPER(qflex_exception_return)(CPUARMState *env) { return; }


#ifndef CONFIG_QFLEX
/* Empty GETTERs in case CONFIG_QFLEX is disabled.
 * To see real functions, see original file (op_helper/helper/helper-a64.c)
 */
uint64_t vaddr_to_paddr(CPUState *cs, uint64_t vaddr, MMUAccessType access_type) {
	return -1;
}
#endif
