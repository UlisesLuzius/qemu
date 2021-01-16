#include "qemu/osdep.h"
#include "cpu.h"
#include "qflex-helper.h"
#include "qflex/qflex.h"

/* qflex/qflex-arch.h
 */
#define ENV(cpu) ((CPUARMState *) cpu->env_ptr)

uint64_t QFLEX_GET_ARCH(pc)(CPUState *cs) { return ENV(cs)->pc; }
int      QFLEX_GET_ARCH(el)(CPUState *cs) { return arm_current_el(ENV(cs)); }


/**
 * NOTE: Layout ttbrN register: (src: https://developer.arm.com/docs/ddi0595/h/aarch64-system-registers/ttbr0_el1)
 * ASID:  bits [63:48]
 * BADDR: bits [47:1]
 * CnP:   bit  [0]
 */
uint64_t QFLEX_GET_ARCH(pid)(CPUState *cs) { 
	int curr_el = arm_current_el(ENV(cs)); // Necessary?
	if(true /* TODO */) {
		return ENV(cs)->cp15.ttbr0_el[curr_el] >> 48;
	} else {
		return ENV(cs)->cp15.ttbr1_el[curr_el] >> 48; 
	}
}

uint64_t gva_to_hva(CPUState *cs, uint64_t addr, int access_type) {
	return gva_to_hva_arch(cs, addr, (MMUAccessType) access_type);
}
