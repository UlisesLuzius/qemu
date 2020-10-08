#ifndef QFLEX_HELPER_H
#define QFLEX_HELPER_H

#include "cpu.h"
#include "internals.h"

/* Functions that are only present as static functions in target/arch files
 * Make them available to get called by QFLEX
 */

#define QFLEX_GET_F(func) glue(qflex_get_, func)

/* helper.c
 */

/* 
 * returns 'NULL' on permission fault or on failure 
 */
uint64_t *vaddr_to_paddr(CPUState *cs, uint64_t vaddr);
#endif /* QFLEX_HELPER_H */
