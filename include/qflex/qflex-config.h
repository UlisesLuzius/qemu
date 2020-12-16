#ifndef QFLEX_CONFIG_H
#define QFLEX_CONFIG_H

#include <stdbool.h>

#include "qemu/osdep.h"

extern QemuOptsList qemu_qflex_opts;
extern QemuOptsList qemu_qflex_model_cache_opts;
extern QemuOptsList qemu_qflex_gen_mem_trace_opts;
 
int qflex_parse_opts(int index, const char *optarg, Error **errp);

#define KIL 1E3
#define MIL 1E6
#define BIL 1E9
void processForOpts(uint64_t *val, const char* qopt, Error **errp);
void processLetterforExponent(uint64_t *val, char c, Error **errp);

#if defined(CONFIG_EXTSNAP) && defined(CONFIG_FLEXUS)
extern QemuOptsList phases_opts;
extern QemuOptsList ckpt_opts;

void configure_phases(QemuOpts *opts, Error **errp);
void configure_ckpt(QemuOpts *opts, Error **errp);
#endif

#endif
