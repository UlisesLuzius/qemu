#include "qemu/osdep.h"
#include "cpu.h"
#include "internals.h"
#include "cpregs.h"
#include "sysemu/cpus.h"

#include "qflex-helper.h"
#include "qflex/qflex.h"
#include "exec/log.h"

uint64_t gva_to_hva(CPUState *cs, uint64_t addr, int access_type) { return gva_to_hva_arch(cs, addr, (MMUAccessType) access_type); }
uint64_t gva_to_hva_with_asid(uint64_t asid_reg, uint64_t vaddr, int access_type) { return gva_to_hva_arch_with_asid(first_cpu, vaddr, (MMUAccessType) access_type, asid_reg); }

int      QEMU_get_num_cores(void) { return 0; /* TODO smp_cores */ }
int      QEMU_get_num_threads_per_core(void) { return 0; /* TODO smp_threads */ }

bool     QFLEX_RD_ARCH(is_idle)(CPUState *cs) { return cpu_thread_is_idle(cs); }
int      QFLEX_RD_ARCH(cpu_index)(CPUState *cs) { return cs->cpu_index; }

void QFLEX_DO_ARCH(log_inst)(CPUState *cs) {
    FILE *logfile = qemu_log_trylock();
    if (logfile) {
        target_disas(logfile, cs, QFLEX_RD_ARCH(pc)(cs), 4);
        qemu_log_unlock(logfile);
    } else {
        printf("Error: logfile busy can't print");
    }
}

bool     QFLEX_RD_ARCH(has_irq)(CPUState *cs) {
    int interrupt_request = cs->interrupt_request;
    CPUARMState *env = cs->env_ptr;
    uint32_t cur_el = arm_current_el(env);
    bool secure = arm_is_secure(env);
    uint64_t hcr_el2 = arm_hcr_el2_eff(env);
    uint32_t target_el;
    uint32_t excp_idx;

    /* The prioritization of interrupts is IMPLEMENTATION DEFINED. */

    if (interrupt_request & CPU_INTERRUPT_FIQ) {
        excp_idx = EXCP_FIQ;
        target_el = arm_phys_excp_target_el(cs, excp_idx, cur_el, secure);
        if (get_arm_excp_unmasked(cs, excp_idx, target_el,
                              cur_el, secure, hcr_el2)) {
            goto found;
        }
    }
    if (interrupt_request & CPU_INTERRUPT_HARD) {
        excp_idx = EXCP_IRQ;
        target_el = arm_phys_excp_target_el(cs, excp_idx, cur_el, secure);
        if (get_arm_excp_unmasked(cs, excp_idx, target_el,
                              cur_el, secure, hcr_el2)) {
            goto found;
        }
    }
    if (interrupt_request & CPU_INTERRUPT_VIRQ) {
        excp_idx = EXCP_VIRQ;
        target_el = 1;
        if (get_arm_excp_unmasked(cs, excp_idx, target_el,
                              cur_el, secure, hcr_el2)) {
            goto found;
        }
    }
    if (interrupt_request & CPU_INTERRUPT_VFIQ) {
        excp_idx = EXCP_VFIQ;
        target_el = 1;
        if (get_arm_excp_unmasked(cs, excp_idx, target_el,
                              cur_el, secure, hcr_el2)) {
            goto found;
        }
    }
    if (interrupt_request & CPU_INTERRUPT_VSERR) {
        excp_idx = EXCP_VSERR;
        target_el = 1;
        if (get_arm_excp_unmasked(cs, excp_idx, target_el,
                              cur_el, secure, hcr_el2)) {
            /* Taking a virtual abort clears HCR_EL2.VSE */
            env->cp15.hcr_el2 &= ~HCR_VSE;
            cpu_reset_interrupt(cs, CPU_INTERRUPT_VSERR);
            goto found;
        }
    }
    return false;

 found:
    return true;
}



/*
 * Aarch64 - get and set state
 */
#define ENV(cpu) ((CPUARMState *) cpu->env_ptr)
#define ARM_ENV(cpu) ((CPUARMState *env = cs->env_ptr;

uint64_t QFLEX_RD_ARCH(pc)(CPUState *cs) { return ENV(cs)->pc; }
int      QFLEX_RD_ARCH(el)(CPUState *cs) { return arm_current_el(ENV(cs)); }
uint64_t QFLEX_RD_ARCH(reg)(CPUState *cs, int reg_index) { assert(reg_index < 32); return ENV(cs)->xregs[reg_index]; }
void     QFLEX_WR_ARCH(reg)(CPUState *cs, int reg_index, uint64_t value) { assert(reg_index < 32); ENV(cs)->xregs[reg_index] = value; }
uint32_t QFLEX_RD_ARCH(pstate)(CPUState *cs) { return pstate_read(ENV(cs)); }
uint32_t QFLEX_RD_ARCH(nzcv)(CPUState *cs) { return (pstate_read(ENV(cs)) >> 28) & 0xF; }
uint64_t QFLEX_RD_ARCH(hcr_el2)(CPUState *cs) { return ENV(cs)->cp15.hcr_el2; }
uint32_t QFLEX_RD_ARCH(exception_syndrome)(CPUState *cs) { return ENV(cs)->exception.syndrome; }
uint32_t QFLEX_RD_ARCH(exception_fsr)(CPUState *cs) { return ENV(cs)->exception.fsr; }
uint64_t QFLEX_RD_ARCH(exception_vaddress)(CPUState *cs) { return ENV(cs)->exception.vaddress; }
uint32_t QFLEX_RD_ARCH(exception_target_el)(CPUState *cs) { return ENV(cs)->exception.target_el; }
uint32_t QFLEX_RD_ARCH(dczid_el0)(CPUState *cs) { return ARM_CPU(cs)->dcz_blocksize; }
uint32_t QFLEX_RD_ARCH(aarch64)(CPUState *cs) { return ENV(cs)->aarch64; }
bool     QFLEX_RD_ARCH(sctlr_el)(CPUState *cs, uint8_t id) { return ENV(cs)->cp15.sctlr_el[id]; }
bool     QFLEX_RD_ARCH(tpidr)(CPUState *cs, uint8_t id) { return ENV(cs)->cp15.tpidr_el[id]; }
uint64_t QFLEX_RD_ARCH(sp_el)(CPUState *cs, uint8_t id) { return ENV(cs)->sp_el[id]; }
bool     QFLEX_RD_ARCH(fpcr)(CPUState *cs) { return vfp_get_fpcr(ENV(cs)); }
bool     QFLEX_RD_ARCH(fpsr)(CPUState *cs) { return vfp_get_fpsr(ENV(cs)); }

/**
 * NOTE: Layout ttbrN register: (src: https://developer.arm.com/docs/ddi0595/h/aarch64-system-registers/ttbr0_el1)
 * ASID:  bits [63:48]
 * BADDR: bits [47:1]
 * CnP:   bit  [0]
 */
uint64_t QFLEX_RD_ARCH(asid)(CPUState *cs) {
    if (false /* TODO: when do we need ttbr1 ? */) {
        return ENV(cs)->cp15.ttbr0_ns >> 48;
    } else {
        return ENV(cs)->cp15.ttbr1_ns >> 48;
    }
}

uint64_t QFLEX_RD_ARCH(asid_reg)(CPUState *cs) {
    if (false /* TODO: when do we need ttbr1 ? */) {
        return ENV(cs)->cp15.ttbr0_ns;
    } else {
        return ENV(cs)->cp15.ttbr1_ns;
    }
}

uint64_t QFLEX_RD_ARCH(tid)(CPUState *cs) {
    int curr_el = arm_current_el(ENV(cs));
    if(curr_el == 0) {
        return ENV(cs)->cp15.tpidrurw_ns;
    } else {
        return ENV(cs)->cp15.tpidrprw_ns;
    }
}

// See helper.c `write_list_to_cpustate` for example
uint64_t QFLEX_RD_ARCH(sysreg)(CPUState *cs, 
                       unsigned int op0, unsigned int op1, unsigned int op2,
                       unsigned int crn, unsigned int crm, unsigned int rt) {
    const ARMCPRegInfo *ri;
    ri = get_arm_cp_reginfo(ARM_CPU(cs)->cp_regs,
                            ENCODE_AA64_CP_REG(CP_REG_ARM64_SYSREG_CP,
                                               crn, crm, op0, op1, op2));
    if(!ri) {
        goto error;
    }

    if (ri->type & ARM_CP_NO_RAW) {
        goto error;
    }

    return read_raw_cp_reg(ENV(cs), ri);

error:
    /* Unknown register; this might be a guest error or a QEMU
     * unimplemented feature.
     */
    qemu_log("QFlex: read access to unsupported AArch64"
             "system register op0:%d op1:%d crn:%d crm:%d op2:%d\n",
             op0, op1, crn, crm, op2);
    assert(false);
    return -1;
}

void qflex_print_state_asid_tid(CPUState* cs) {
    CPUARMState *env = cs->env_ptr;
    qemu_log("[MEM]CPU%" PRIu32 "\n"
             "  TTBR0:0[0x%04" PRIx64 "]:1[0x%4" PRIx64 "]:2[0x%4" PRIx64 "]:3[0x%4" PRIx64 "]\n"
             "  TTBR1:0[0x%04" PRIx64 "]:1[0x%4" PRIx64 "]:2[0x%4" PRIx64 "]:3[0x%4" PRIx64 "]\n"
             "  CXIDR:0[0x%04" PRIx64 "]:1[0x%4" PRIx64 "]:2[0x%4" PRIx64 "]:3[0x%4" PRIx64 "]\n"
             "  TPIDR:0[0x%016" PRIx64 "]:1[0x%016" PRIx64 "]:2[0x%016" PRIx64 "]:3[0x%016" PRIx64 "]\n"
             "  TPIDX:0[0x%016" PRIx64 "]:R[0x%016" PRIx64 "]\n",
             cs->cpu_index,
             env->cp15.ttbr0_el[0], env->cp15.ttbr0_el[1], env->cp15.ttbr0_el[2], env->cp15.ttbr0_el[3],
             env->cp15.ttbr1_el[0], env->cp15.ttbr1_el[1], env->cp15.ttbr1_el[2], env->cp15.ttbr1_el[3],
             env->cp15.contextidr_el[0], env->cp15.contextidr_el[1], env->cp15.contextidr_el[2], env->cp15.contextidr_el[3],
             env->cp15.tpidr_el[0], env->cp15.tpidr_el[1], env->cp15.tpidr_el[2], env->cp15.tpidr_el[3],
             env->cp15.tpidruro_ns, env->cp15.tpidrro_el[0]);
}

void qflex_dump_archstate_log(CPUState *cpu) {
    qemu_log("ASID[0x%08lx]:PC[0x%016lx]\n", QFLEX_RD_ARCH(asid)(cpu), QFLEX_RD_ARCH(pc)(cpu));
    for (int reg = 0; reg < 32; reg++) {
        qemu_log("X%02i[%016lx]\n", reg, QFLEX_RD_ARCH(reg)(cpu, reg));
    }
}