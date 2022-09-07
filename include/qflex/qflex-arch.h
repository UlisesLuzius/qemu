#ifndef QFLEX_ARCH_H
#define QFLEX_ARCH_H

#include "qemu/osdep.h"

/* This file contains functions specific to architectural state.
 * These functions should be defined in target/arch/qflex-helper.c in
 * order to access architecture specific values
 */

// functions define to get CPUArchState specific values
#define QFLEX_DO_ARCH(name) glue(QFLEX_DO_ARCH_, name)
#define QFLEX_RD_ARCH(name) glue(QFLEX_RD_ARCH_, name)
#define QFLEX_WR_ARCH(name) glue(QFLEX_WR_ARCH_, name)

uint64_t gva_to_hva(CPUState *cs, uint64_t addr, int access_type);
uint64_t gva_to_hva_with_asid(uint64_t asid_reg, uint64_t vaddr, int access_type);

int      QEMU_get_num_cpus(void);
int      QEMU_get_num_cores(void);
int      QEMU_get_num_threads_per_core(void);

bool     QFLEX_RD_ARCH(is_idle)(CPUState *cs);
int      QFLEX_RD_ARCH(cpu_index)(CPUState *cs);

void	 QFLEX_DO_ARCH(log_inst)(CPUState *cs);
bool     QFLEX_RD_ARCH(has_irq)(CPUState *cs);


uint64_t QFLEX_RD_ARCH(pc)(CPUState *cs);
int      QFLEX_RD_ARCH(el)(CPUState *cs);
uint64_t QFLEX_RD_ARCH(reg)(CPUState *cs, int reg_index);
void     QFLEX_WR_ARCH(reg)(CPUState *cs, int reg_index, uint64_t value);
uint32_t QFLEX_RD_ARCH(pstate)(CPUState *cs);
uint32_t QFLEX_RD_ARCH(nzcv)(CPUState *cs);
uint64_t QFLEX_RD_ARCH(hcr_el2)(CPUState *cs);
uint32_t QFLEX_RD_ARCH(exception_syndrome)(CPUState *cs);
uint32_t QFLEX_RD_ARCH(exception_fsr)(CPUState *cs);
uint64_t QFLEX_RD_ARCH(exception_vaddress)(CPUState *cs);
uint32_t QFLEX_RD_ARCH(exception_target_el)(CPUState *cs);
uint32_t QFLEX_RD_ARCH(dczid_el0)(CPUState *cs);
uint32_t QFLEX_RD_ARCH(aarch64)(CPUState *cs);
bool     QFLEX_RD_ARCH(sctlr_el)(CPUState *cs, uint8_t id);
bool     QFLEX_RD_ARCH(tpidr)(CPUState *cs, uint8_t id);
uint64_t QFLEX_RD_ARCH(sp_el)(CPUState *cs, uint8_t id);
bool     QFLEX_RD_ARCH(fpcr)(CPUState *cs);
bool     QFLEX_RD_ARCH(fpsr)(CPUState *cs);

uint64_t QFLEX_RD_ARCH(asid)(CPUState *cs);
uint64_t QFLEX_RD_ARCH(asid_reg)(CPUState *cs);
uint64_t QFLEX_RD_ARCH(tid)(CPUState *cs);
// See helper.c `write_list_to_cpustate` for example
uint64_t QFLEX_RD_ARCH(sysreg)(CPUState *cs, 
                       unsigned int op0, unsigned int op1, unsigned int op2,
                       unsigned int crn, unsigned int crm, unsigned int rt);

void qflex_dump_archstate_log(CPUState *cpu);


#endif /* QFLEX_ARCH_H */
