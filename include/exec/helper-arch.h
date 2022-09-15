#ifndef HELPER_ARCH_H
#define HELPER_ARCH_H

#include <stdbool.h>
#include "exec/helper-head.h"

#define ENV(cpu) ((CPUArchState *) cpu->env_ptr)

bool HELPER(vcpu_is_userland)(CPUState *cpu);

#endif /* HELPER_ARCH_H */