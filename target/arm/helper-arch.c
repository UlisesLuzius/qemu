#include "qemu/osdep.h"
#include "cpu.h"
#include "exec/helper-proto.h"
#include "exec/log.h"
#include "exec/exec-all.h"
#include "exec/helper-arch.h"


bool HELPER(vcpu_is_userland)(CPUState *cpu) { return arm_current_el(ENV(cpu)) == 0; }
