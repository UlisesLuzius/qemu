#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"

#include "qflex/qflex-arch.h"
#include "qflex/devteroflex/devteroflex.h"

/** 
 * NOTE: Functions that need to access architecture specific state (e.g. 
 *       registers) should be located here. You can find in the structure 
 *       CPUARMState all state relevant.
 */

void devteroflex_pack_archstate(DevteroflexArchState *devteroflex, CPUState *cpu ) {
    CPUARMState *env = cpu->env_ptr;

    memcpy(&devteroflex->xregs, &env->xregs, 32*sizeof(uint64_t));
    devteroflex->pc = QFLEX_GET_ARCH(pc)(cpu);
    devteroflex->asid = QFLEX_GET_ARCH(asid)(cpu);
    FLAGS_SET_NZCV(devteroflex->flags, QFLEX_GET_ARCH(nzcv)(cpu));

    devteroflex->icount = devteroflexConfig.icount;
    devteroflex->icountExecuted = 0;
    devteroflex->icountBudget = cpu->icount_budget;
}

void devteroflex_unpack_archstate(CPUState *cpu, DevteroflexArchState *devteroflex) {
    CPUARMState *env = cpu->env_ptr;

    memcpy(&env->xregs, &devteroflex->xregs, 32*sizeof(uint64_t));
    env->pc = devteroflex->pc;
    assert(QFLEX_GET_ARCH(asid)(cpu) == devteroflex->asid);

    uint32_t nzcv = FLAGS_GET_NZCV(devteroflex->flags);
    uint32_t cpu_pstate = pstate_read(env);
    uint32_t new_pstate = (cpu_pstate & ~(0xF << 28)) | (nzcv << 28);
    pstate_write(env, new_pstate);

    devteroflex->icount = devteroflexConfig.icount;
    icount_update_devteroflex_executed(cpu, devteroflex->icountExecuted);
}

bool devteroflex_compare_archstate(CPUState *cpu, DevteroflexArchState *devteroflex) {
    bool hasMismatch = false;
    if(QFLEX_GET_ARCH(pc)(cpu) != devteroflex->pc) {
        qemu_log("Mismatched register PC is detected. \n QEMU: %lx, FPGA: %lx \n", QFLEX_GET_ARCH(pc)(cpu), devteroflex->pc);
        hasMismatch = true;
    }

    for(int i = 0; i < 32; ++i) {
        if(QFLEX_GET_ARCH(reg)(cpu, i) != devteroflex->xregs[i]) {
            qemu_log("Mismatched register %d is detected. \n QEMU: %lx, FPGA: %lx \n", i, QFLEX_GET_ARCH(reg)(cpu, i), devteroflex->xregs[i]);
            hasMismatch = true;
        }
    }
    
    if(QFLEX_GET_ARCH(nzcv)(cpu) != FLAGS_GET_NZCV(devteroflex->flags)) {
        qemu_log("Mismatched register NZCV is detected. \n QEMU: %x, FPGA: %lx \n", QFLEX_GET_ARCH(nzcv)(cpu), FLAGS_GET_NZCV(devteroflex->flags));
        hasMismatch = true;
    }

    if(QFLEX_GET_ARCH(asid)(cpu) != devteroflex->asid) {
        qemu_log("Mismatched register ASID is detected. \n QEMU: %lx, FPGA: %x \n", QFLEX_GET_ARCH(asid)(cpu), devteroflex->asid);
        hasMismatch = true;
    }

    if(hasMismatch) {
        qflex_dump_archstate_log(cpu);
    }

    return hasMismatch;
}

