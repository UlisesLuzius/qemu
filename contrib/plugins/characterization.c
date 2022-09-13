/*
 * Copyright (C) 2021, Mahmoud Mandour <ma.mandourr@gmail.com>
 *
 * License: GNU GPL, version 2 or later.
 *   See the COPYING file in the top-level directory.
 */

#include <inttypes.h>
#include <stdio.h>
#include <glib.h>

#include <qemu-plugin.h>

#define STRTOLL(x) g_ascii_strtoll(x, NULL, 10)

QEMU_PLUGIN_EXPORT int qemu_plugin_version = QEMU_PLUGIN_VERSION;

static enum qemu_plugin_mem_rw rw = QEMU_PLUGIN_MEM_RW;

static void vcpu_mem_access(unsigned int vcpu_index, qemu_plugin_meminfo_t info,
                            uint64_t vaddr, void *userdata)
{
    uint64_t effective_addr;
    struct qemu_plugin_hwaddr *hwaddr;
    int cache_idx;
    InsnData *insn;
    bool hit_in_l1;

    hwaddr = qemu_plugin_get_hwaddr(info, vaddr);
    if (hwaddr && qemu_plugin_hwaddr_is_io(hwaddr)) {
        return;
    }

    effective_addr = hwaddr ? qemu_plugin_hwaddr_phys_addr(hwaddr) : vaddr;
}

static void vcpu_insn_exec(unsigned int vcpu_index, void *userdata)
{
    uint64_t insn_addr;
    InsnData *insn;
    int cache_idx;
    bool hit_in_l1;

    insn_addr = ((InsnData *) userdata)->addr;
}

static void vcpu_tb_trans(qemu_plugin_id_t id, struct qemu_plugin_tb *tb)
{
    size_t n_insns;
    size_t i;
    InsnData *data;

    n_insns = qemu_plugin_tb_n_insns(tb);
    for (i = 0; i < n_insns; i++) {
        struct qemu_plugin_insn *insn = qemu_plugin_tb_get_insn(tb, i);
        uint64_t effective_addr;

        if (sys) {
            effective_addr = (uint64_t) qemu_plugin_insn_haddr(insn);
        } else {
            effective_addr = (uint64_t) qemu_plugin_insn_vaddr(insn);
        }

        /*
         * Instructions might get translated multiple times, we do not create
         * new entries for those instructions. Instead, we fetch the same
         * entry from the hash table and register it for the callback again.
         */
        g_mutex_lock(&hashtable_lock);
        data = g_hash_table_lookup(miss_ht, GUINT_TO_POINTER(effective_addr));
        if (data == NULL) {
            data = g_new0(InsnData, 1);
            data->disas_str = qemu_plugin_insn_disas(insn);
            data->symbol = qemu_plugin_insn_symbol(insn);
            data->addr = effective_addr;
            g_hash_table_insert(miss_ht, GUINT_TO_POINTER(effective_addr),
                               (gpointer) data);
        }
        g_mutex_unlock(&hashtable_lock);

        qemu_plugin_register_vcpu_mem_cb(insn, vcpu_mem_access,
                                         QEMU_PLUGIN_CB_NO_REGS,
                                         rw, data);

        qemu_plugin_register_vcpu_insn_exec_cb(insn, vcpu_insn_exec,
                                               QEMU_PLUGIN_CB_NO_REGS, data);
    }
}

static void insn_free(gpointer data)
{
    InsnData *insn = (InsnData *) data;
    g_free(insn->disas_str);
    g_free(insn);
}

static void cache_free(Cache *cache)
{
    for (int i = 0; i < cache->num_sets; i++) {
        g_free(cache->sets[i].blocks);
    }

    if (metadata_destroy) {
        metadata_destroy(cache);
    }

    g_free(cache->sets);
    g_free(cache);
}

static void caches_free(Cache **caches)
{
    int i;

    for (i = 0; i < cores; i++) {
        cache_free(caches[i]);
    }
}

static void append_stats_line(GString *line, uint64_t l1_daccess,
                              uint64_t l1_dmisses, uint64_t l1_iaccess,
                              uint64_t l1_imisses,  uint64_t l2_access,
                              uint64_t l2_misses)
{
    double l1_dmiss_rate, l1_imiss_rate, l2_miss_rate;

    l1_dmiss_rate = ((double) l1_dmisses) / (l1_daccess) * 100.0;
    l1_imiss_rate = ((double) l1_imisses) / (l1_iaccess) * 100.0;

    g_string_append_printf(line, "%-14lu %-12lu %9.4lf%%  %-14lu %-12lu"
                           " %9.4lf%%",
                           l1_daccess,
                           l1_dmisses,
                           l1_daccess ? l1_dmiss_rate : 0.0,
                           l1_iaccess,
                           l1_imisses,
                           l1_iaccess ? l1_imiss_rate : 0.0);

    if (use_l2) {
        l2_miss_rate =  ((double) l2_misses) / (l2_access) * 100.0;
        g_string_append_printf(line, "  %-12lu %-11lu %10.4lf%%",
                               l2_access,
                               l2_misses,
                               l2_access ? l2_miss_rate : 0.0);
    }

    g_string_append(line, "\n");
}

static void sum_stats(void)
{
    int i;

    g_assert(cores > 1);
    for (i = 0; i < cores; i++) {
        l1_imisses += l1_icaches[i]->misses;
        l1_dmisses += l1_dcaches[i]->misses;
        l1_imem_accesses += l1_icaches[i]->accesses;
        l1_dmem_accesses += l1_dcaches[i]->accesses;

        if (use_l2) {
            l2_misses += l2_ucaches[i]->misses;
            l2_mem_accesses += l2_ucaches[i]->accesses;
        }
    }
}

static int dcmp(gconstpointer a, gconstpointer b)
{
    InsnData *insn_a = (InsnData *) a;
    InsnData *insn_b = (InsnData *) b;

    return insn_a->l1_dmisses < insn_b->l1_dmisses ? 1 : -1;
}

static int icmp(gconstpointer a, gconstpointer b)
{
    InsnData *insn_a = (InsnData *) a;
    InsnData *insn_b = (InsnData *) b;

    return insn_a->l1_imisses < insn_b->l1_imisses ? 1 : -1;
}

static int l2_cmp(gconstpointer a, gconstpointer b)
{
    InsnData *insn_a = (InsnData *) a;
    InsnData *insn_b = (InsnData *) b;

    return insn_a->l2_misses < insn_b->l2_misses ? 1 : -1;
}

static void log_stats(void)
{
    int i;
    Cache *icache, *dcache, *l2_cache;

    g_autoptr(GString) rep = g_string_new("core #, data accesses, data misses,"
                                          " dmiss rate, insn accesses,"
                                          " insn misses, imiss rate");

    if (use_l2) {
        g_string_append(rep, ", l2 accesses, l2 misses, l2 miss rate");
    }

    g_string_append(rep, "\n");

    for (i = 0; i < cores; i++) {
        g_string_append_printf(rep, "%-8d", i);
        dcache = l1_dcaches[i];
        icache = l1_icaches[i];
        l2_cache = use_l2 ? l2_ucaches[i] : NULL;
        append_stats_line(rep, dcache->accesses, dcache->misses,
                icache->accesses, icache->misses,
                l2_cache ? l2_cache->accesses : 0,
                l2_cache ? l2_cache->misses : 0);
    }

    if (cores > 1) {
        sum_stats();
        g_string_append_printf(rep, "%-8s", "sum");
        append_stats_line(rep, l1_dmem_accesses, l1_dmisses,
                l1_imem_accesses, l1_imisses,
                l2_cache ? l2_mem_accesses : 0, l2_cache ? l2_misses : 0);
    }

    g_string_append(rep, "\n");
    qemu_plugin_outs(rep->str);
}

static void log_top_insns(void)
{
    int i;
    GList *curr, *miss_insns;
    InsnData *insn;

    miss_insns = g_hash_table_get_values(miss_ht);
    miss_insns = g_list_sort(miss_insns, dcmp);
    g_autoptr(GString) rep = g_string_new("");
    g_string_append_printf(rep, "%s", "address, data misses, instruction\n");

    for (curr = miss_insns, i = 0; curr && i < limit; i++, curr = curr->next) {
        insn = (InsnData *) curr->data;
        g_string_append_printf(rep, "0x%" PRIx64, insn->addr);
        if (insn->symbol) {
            g_string_append_printf(rep, " (%s)", insn->symbol);
        }
        g_string_append_printf(rep, ", %ld, %s\n", insn->l1_dmisses,
                               insn->disas_str);
    }

    miss_insns = g_list_sort(miss_insns, icmp);
    g_string_append_printf(rep, "%s", "\naddress, fetch misses, instruction\n");

    for (curr = miss_insns, i = 0; curr && i < limit; i++, curr = curr->next) {
        insn = (InsnData *) curr->data;
        g_string_append_printf(rep, "0x%" PRIx64, insn->addr);
        if (insn->symbol) {
            g_string_append_printf(rep, " (%s)", insn->symbol);
        }
        g_string_append_printf(rep, ", %ld, %s\n", insn->l1_imisses,
                               insn->disas_str);
    }

    if (!use_l2) {
        goto finish;
    }

    miss_insns = g_list_sort(miss_insns, l2_cmp);
    g_string_append_printf(rep, "%s", "\naddress, L2 misses, instruction\n");

    for (curr = miss_insns, i = 0; curr && i < limit; i++, curr = curr->next) {
        insn = (InsnData *) curr->data;
        g_string_append_printf(rep, "0x%" PRIx64, insn->addr);
        if (insn->symbol) {
            g_string_append_printf(rep, " (%s)", insn->symbol);
        }
        g_string_append_printf(rep, ", %ld, %s\n", insn->l2_misses,
                               insn->disas_str);
    }

finish:
    qemu_plugin_outs(rep->str);
    g_list_free(miss_insns);
}

static void plugin_exit(qemu_plugin_id_t id, void *p)
{
    log_stats();
    log_top_insns();

    caches_free(l1_dcaches);
    caches_free(l1_icaches);

    g_free(l1_dcache_locks);
    g_free(l1_icache_locks);

    if (use_l2) {
        caches_free(l2_ucaches);
        g_free(l2_ucache_locks);
    }

    g_hash_table_destroy(miss_ht);
}

static void policy_init(void)
{
    switch (policy) {
    case LRU:
        update_hit = lru_update_blk;
        update_miss = lru_update_blk;
        metadata_init = lru_priorities_init;
        metadata_destroy = lru_priorities_destroy;
        break;
    case FIFO:
        update_miss = fifo_update_on_miss;
        metadata_init = fifo_init;
        metadata_destroy = fifo_destroy;
        break;
    case RAND:
        rng = g_rand_new();
        break;
    default:
        g_assert_not_reached();
    }
}

QEMU_PLUGIN_EXPORT
int qemu_plugin_install(qemu_plugin_id_t id, const qemu_info_t *info,
                        int argc, char **argv)
{
    int i;
    int l1_iassoc, l1_iblksize, l1_icachesize;
    int l1_dassoc, l1_dblksize, l1_dcachesize;
    int l2_assoc, l2_blksize, l2_cachesize;

    limit = 32;
    sys = info->system_emulation;

    l1_dassoc = 8;
    l1_dblksize = 64;
    l1_dcachesize = l1_dblksize * l1_dassoc * 32;

    l1_iassoc = 8;
    l1_iblksize = 64;
    l1_icachesize = l1_iblksize * l1_iassoc * 32;

    l2_assoc = 16;
    l2_blksize = 64;
    l2_cachesize = l2_assoc * l2_blksize * 2048;

    policy = LRU;

    cores = sys ? qemu_plugin_n_vcpus() : 1;

    for (i = 0; i < argc; i++) {
        char *opt = argv[i];
        g_autofree char **tokens = g_strsplit(opt, "=", 2);

        if (g_strcmp0(tokens[0], "iblksize") == 0) {
            l1_iblksize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "iassoc") == 0) {
            l1_iassoc = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "icachesize") == 0) {
            l1_icachesize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "dblksize") == 0) {
            l1_dblksize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "dassoc") == 0) {
            l1_dassoc = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "dcachesize") == 0) {
            l1_dcachesize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "limit") == 0) {
            limit = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "cores") == 0) {
            cores = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "l2cachesize") == 0) {
            use_l2 = true;
            l2_cachesize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "l2blksize") == 0) {
            use_l2 = true;
            l2_blksize = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "l2assoc") == 0) {
            use_l2 = true;
            l2_assoc = STRTOLL(tokens[1]);
        } else if (g_strcmp0(tokens[0], "l2") == 0) {
            if (!qemu_plugin_bool_parse(tokens[0], tokens[1], &use_l2)) {
                fprintf(stderr, "boolean argument parsing failed: %s\n", opt);
                return -1;
            }
        } else if (g_strcmp0(tokens[0], "evict") == 0) {
            if (g_strcmp0(tokens[1], "rand") == 0) {
                policy = RAND;
            } else if (g_strcmp0(tokens[1], "lru") == 0) {
                policy = LRU;
            } else if (g_strcmp0(tokens[1], "fifo") == 0) {
                policy = FIFO;
            } else {
                fprintf(stderr, "invalid eviction policy: %s\n", opt);
                return -1;
            }
        } else {
            fprintf(stderr, "option parsing failed: %s\n", opt);
            return -1;
        }
    }

    policy_init();

    l1_dcaches = caches_init(l1_dblksize, l1_dassoc, l1_dcachesize);
    if (!l1_dcaches) {
        const char *err = cache_config_error(l1_dblksize, l1_dassoc, l1_dcachesize);
        fprintf(stderr, "dcache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return -1;
    }

    l1_icaches = caches_init(l1_iblksize, l1_iassoc, l1_icachesize);
    if (!l1_icaches) {
        const char *err = cache_config_error(l1_iblksize, l1_iassoc, l1_icachesize);
        fprintf(stderr, "icache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return -1;
    }

    l2_ucaches = use_l2 ? caches_init(l2_blksize, l2_assoc, l2_cachesize) : NULL;
    if (!l2_ucaches && use_l2) {
        const char *err = cache_config_error(l2_blksize, l2_assoc, l2_cachesize);
        fprintf(stderr, "L2 cache cannot be constructed from given parameters\n");
        fprintf(stderr, "%s\n", err);
        return -1;
    }

    l1_dcache_locks = g_new0(GMutex, cores);
    l1_icache_locks = g_new0(GMutex, cores);
    l2_ucache_locks = use_l2 ? g_new0(GMutex, cores) : NULL;

    qemu_plugin_register_vcpu_tb_trans_cb(id, vcpu_tb_trans);
    qemu_plugin_register_atexit_cb(id, plugin_exit, NULL);

    miss_ht = g_hash_table_new_full(NULL, g_direct_equal, NULL, insn_free);
    
    printf("Plugin setup done");

    return 0;
}
