#include "qemu/osdep.h"
#include "cpu.h"

#include "qflex/qflex-arch.h"
#include "qflex/armflex.h"

void armflex_pack_archstate(CPUState *cpu, ArmflexArchState *armflex) {
    CPUARMState *env = cpu->env_ptr;

	memcpy(&armflex->xregs,     &env->xregs, 32*sizeof(uint64_t));
    armflex->pc = env->pc;
    armflex->sp = env->sp_el[QFLEX_GET_ARCH(el)(cpu)];

    uint64_t nzcv =
		((env->CF)           ? 1 << ARCH_PSTATE_CF_MASK : 0) |
		((env->VF & (1<<31)) ? 1 << ARCH_PSTATE_VF_MASK : 0) |
		((env->NF & (1<<31)) ? 1 << ARCH_PSTATE_NF_MASK : 0) |
		(!(env->ZF)          ? 1 << ARCH_PSTATE_ZF_MASK : 0);
	armflex->nzcv = nzcv;
}

void armflex_unpack_archstate(ArmflexArchState *armflex, CPUState *cpu) {
    CPUARMState *env = cpu->env_ptr;

    memcpy(&env->xregs, &armflex->xregs, 32*sizeof(uint64_t));
    env->pc = armflex->pc;
    env->sp_el[QFLEX_GET_ARCH(el)(cpu)] = armflex->sp;

    uint32_t nzcv = armflex->nzcv;
    env->CF = (nzcv & ARCH_PSTATE_CF_MASK) ? 1 : 0;
    env->VF = (nzcv & ARCH_PSTATE_VF_MASK) ? (1 << 31) : 0;
    env->NF = (nzcv & ARCH_PSTATE_NF_MASK) ? (1 << 31) : 0;
    env->ZF = !(nzcv & ARCH_PSTATE_ZF_MASK) ? 1 : 0;
}
