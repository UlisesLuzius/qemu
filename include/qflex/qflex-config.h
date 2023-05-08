#ifndef QFLEX_CONFIG_H
#define QFLEX_CONFIG_H

#include "qemu/osdep.h"

extern QemuOptsList qemu_qflex_opts;
extern QemuOptsList qemu_qflex_model_cache_opts;
extern QemuOptsList qemu_qflex_gen_mem_trace_opts;

int qflex_parse_opts(int index, const char *optarg, Error **errp);
void qflex_init(Error **errp);
 
#endif
