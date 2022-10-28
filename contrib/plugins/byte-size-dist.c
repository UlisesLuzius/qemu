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
static size_t totalNops[2][32] = {0};

static void vcpu_insn_exec(unsigned int vcpu_index, void *encoded)
{
    size_t byteSize = (size_t) encoded & 0xFF;
    bool is_user = ((size_t) encoded & 1 << 16);
    bool is_nop = ((size_t) encoded & 1 << 17);
    if(is_user) {
        byteSizeDist[0][vcpu_index][byteSize]++;
        totalNops[1][vcpu_index] += is_nop ? 1 : 0;
    } else {
        byteSizeDist[1][vcpu_index][byteSize]++;
        totalNops[1][vcpu_index] += is_nop ? 1 : 0;
    }

    totInsn++;
    if((totInsn % 1000000000) == 0) {
        g_autoptr(GString) rep = g_string_new("cpu,byte,user,kernel");
        g_string_append_printf(rep, "[%016ld]\n", totInsn); 
        uint64_t tot_insn = 0, tot_user = 0;
#ifdef CONFIG_1
        for(int cpu = 1; cpu < 2; cpu++) {
#else
        for(int cpu = 0; cpu < 16; cpu++) {
#endif
            tot_insn = 0;
            tot_user = 0;
            for(int insnSize = 0; insnSize < 16; insnSize++) {
                tot_insn += byteSizeDist[0][cpu][insnSize] + byteSizeDist[1][cpu][insnSize];
                tot_user += byteSizeDist[0][cpu][insnSize];
                g_string_append_printf(rep, "%u,%u,%016ld,%016ld\n", 
                                   cpu, insnSize, byteSizeDist[0][cpu][insnSize],
                                   byteSizeDist[1][cpu][insnSize], byteSizeDist);
            }
            double user_ratio = ((double) tot_user) / tot_insn * 100.0;
            g_string_append_printf(rep, "user/kernel,%i,%10.4lf\n", cpu, user_ratio);
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
        bool is_nop = (strcmp(qemu_plugin_insn_symbol(insn), "nop") == 0);
        bool is_user = qemu_plugin_is_userland(insn);
        size_t encoded = is_user << 16 | is_nop << 17 | bytesize;
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
