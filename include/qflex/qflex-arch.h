#ifndef QFLEX_ARCH_H
#define QFLEX_ARCH_H

#include "qemu/osdep.h"

/* This file contains functions specific to architectural state.
 * These functions should be defined in target/arch/qflex-helper.c in
 * order to access architecture specific values
 */

// functions define to get CPUArchState specific values
#define QFLEX_GET_ARCH(name) glue(qflex_get_arch_, name)

uint64_t QFLEX_GET_ARCH(pc)(CPUState *cs);
int QFLEX_GET_ARCH(el)(CPUState *cs);
uint64_t QFLEX_GET_ARCH(pid)(CPUState *cs) 

#endif /* QFLEX_ARCH_H */
