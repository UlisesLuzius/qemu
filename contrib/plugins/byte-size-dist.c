/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <unistd.h>
#include <inttypes.h>
#include <stdio.h>
#include <glib.h>
#include <pthread.h>
#include "cache-parallel.h"

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static size_t totInsn = 0;
static size_t byteSizeDist[32][16] = {0};

static void vcpu_insn_exec(unsigned int vcpu_index, void *bytesize_val)
{
    size_t byteSize = (size_t) bytesize_val;
    byteSizeDist[vcpu_index][byteSize]++;
    totInsn++;
    if((totInsn % 1000000000) == 0) {
        g_autoptr(GString) rep = g_string_new("byteSizeDisk:\n");
        for(int cpu = 0; cpu < 16; cpu++) {
            for(int insnSize = 0; insnSize < 16; insnSize++) {
                g_string_append_printf(rep, "%u,%u,%016ld\n", 
                                   vcpu_index, insnSize, byteSizeDist[vcpu_index][insnSize]);

            }
        }
        qemu_plugin_outs(rep->str);
    }
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    size_t i;

    n_insns = qemu_plugin_tb_n_insns(tb);
    for (i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        size_t bytesize = qemu_plugin_insn_size(insn);
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, (void *) bytesize);
    }
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    return 0;
}
