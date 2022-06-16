#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/config-file.h"
#include "qemu/qemu-options.h"
#include "qemu/main-loop.h"

#include "sysemu/tcg.h"

#include "qflex/qflex-config.h"
#include "qflex/qflex.h"
#include "qflex/qflex-log.h"
#include "qflex/qflex-traces.h"

QemuOptsList qemu_qflex_opts = {
    .name = "qflex",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_qflex_opts.head),
    .desc = {
        {
            .name = "singlestep",
            .type = QEMU_OPT_BOOL,

        },
        { /* end of list */ }
    },
};

QemuOptsList qemu_qflex_gen_mem_trace_opts = {
    .name = "qflex-gen-mem-trace",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_qflex_gen_mem_trace_opts.head),
    .desc = {
        {
            .name = "core_count",
            .type = QEMU_OPT_NUMBER,
        },
        { /* end of list */ }
    },
};

#ifdef CONFIG_DEVTEROFLEX
#include "qflex/devteroflex/devteroflex.h"
QemuOptsList qemu_devteroflex_opts = {
    .name = "devteroflex",
    .implied_opt_name = "enabled",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_devteroflex_opts.head),
    .desc = {
        {
            .name = "enabled",
            .type = QEMU_OPT_BOOL,
        },
        {
            .name = "dram-pages",
            .type = QEMU_OPT_NUMBER,
        },
        {
            .name = "debug",
            .type = QEMU_OPT_STRING,
        },
        {
            .name = "pure-singlestep",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static void devteroflex_configure(QemuOpts *opts, Error **errp) {
    bool enabled = qemu_opt_get_bool(opts, "enabled", false);
    uint64_t fpga_dram_size = qemu_opt_get_number(opts, "dram-pages", -1);
    const char *debug_mode_str = qemu_opt_get(opts, "debug");
    int debug_mode = no_debug;
    if(!debug_mode_str) {
        debug_mode = no_debug;
    } else if(!strcmp(debug_mode_str, "no_debug")) {
        debug_mode = no_debug;
    } else if (!strcmp(debug_mode_str, "mem_sync")) {
        debug_mode = mem_sync;
    } else if (!strcmp(debug_mode_str, "no_mem_sync")) {
        debug_mode = no_mem_sync;
    } else {
        fprintf(stderr, "Unknown debug argument:`%s, please pick 'no_debug', 'mem_sync', 'no_mem_sync'\n", debug_mode_str);
        exit(1);
    }
    bool pure_singlestep = qemu_opt_get_bool(opts, "pure-singlestep", false);
    devteroflex_init(enabled, false, fpga_dram_size, debug_mode, pure_singlestep);
}
#endif /* CONFIG_DEVTEROFLEX */

static void qflex_configure(QemuOpts *opts, Error **errp) {
    qflexState.singlestep = qemu_opt_get_bool(opts, "singlestep", false);
    if (qflexState.singlestep) {
        int error = 0;
        if (!tcg_enabled()) {
            error_report("`singlestep` available only with TCG.");
            error |= 1;
        }
        if (!qemu_loglevel_mask(CPU_LOG_TB_NOCHAIN)) {
            error_report("`singlestep` available only with `-d nochain`.");
            error |= 1;
        }
        if (error)
            exit(EXIT_FAILURE);
    }
}

static void qflex_log_configure(const char *opts) {
    int mask;
    mask = qflex_str_to_log_mask(opts);
    if (!mask) {
        qflex_print_log_usage(opts, stdout);
        exit(EXIT_FAILURE);
    }
    qflex_set_log(mask);
}

static void qflex_gen_mem_trace_configure(QemuOpts *opts, Error **errp) {
    int core_count = qemu_opt_get_number(opts, "core_count", 1);
	qflex_mem_trace_init(core_count);
}

int qflex_parse_opts(int index, const char *optarg, Error **errp) {
    QemuOpts *opts;

    switch(index) {
    case QEMU_OPTION_qflex:
        opts = qemu_opts_parse_noisily(
            qemu_find_opts("qflex"), optarg, false);
        if (!opts) { exit(EXIT_FAILURE); }
        qflex_configure(opts, errp);
        qemu_opts_del(opts);
        break;
    case QEMU_OPTION_qflex_d:
        qflex_log_configure(optarg);
        break;
    case QEMU_OPTION_qflex_gen_mem_trace:
        opts = qemu_opts_parse_noisily(
                qemu_find_opts("qflex-gen-mem-trace"), optarg, false);
        if(!opts) { exit(EXIT_FAILURE); }
        qflex_gen_mem_trace_configure(opts, errp);
        qemu_opts_del(opts);
        break;
#ifdef CONFIG_DEVTEROFLEX
	case QEMU_OPTION_devteroflex:
		opts = qemu_opts_parse_noisily(qemu_find_opts("devteroflex"),
											   optarg, false);
		if (!opts) { exit(EXIT_FAILURE); }
		devteroflex_configure(opts, errp);
        qemu_opts_del(opts);
		break;
#endif /* CONFIG_DEVTEROFLEX */
    default:
        return 0;
    }
    return 1;
}
