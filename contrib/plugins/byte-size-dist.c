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
static size_t byteSizeDist[2][32][16] = {0};

static void vcpu_insn_exec(unsigned int vcpu_index, void *encoded)
{
    size_t byteSize = (size_t) encoded & 0xFF;
    bool is_user = ((size_t) encoded & 1 << 16);
    if(is_user) {
        byteSizeDist[0][vcpu_index][byteSize]++;
    } else {
        byteSizeDist[1][vcpu_index][byteSize]++;
    }

    totInsn++;
    if((totInsn % 1000000000) == 0) {
        g_autoptr(GString) rep = g_string_new("cpu,byte,user,kernel");
        g_string_append_printf(rep, "[%016ld]\n", totInsn); 
#ifdef CONFIG_1
        for(int cpu = 1; cpu < 2; cpu++) {
#else
        for(int cpu = 0; cpu < 16; cpu++) {
#endif
            for(int insnSize = 0; insnSize < 16; insnSize++) {
                g_string_append_printf(rep, "%u,%u,%016ld,%016ld\n", 
                                   cpu, insnSize, byteSizeDist[0][cpu][insnSize],
                                   byteSizeDist[1][cpu][insnSize]);

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
        size_t encoded = qemu_plugin_is_userland(insn) << 16 | bytesize;
        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, (void *) encoded);
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
