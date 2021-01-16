#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/config-file.h"
#include "qemu-options.h"
#include "qemu/main-loop.h"

#include "qflex/qflex-config.h"
#include "qflex/qflex.h"
#include "qflex/qflex-log.h"
#include "qflex/qflex-models.h"

QemuOptsList qemu_qflex_opts = {
    .name = "qflex",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_qflex_opts.head),
    .desc = {
        {
            .name = "ff",
            .type = QEMU_OPT_BOOL,
        }, {
            .name = "profile",
            .type = QEMU_OPT_BOOL,
        }, {
            .name = "profile-mode",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "pth_iloop",
            .type = QEMU_OPT_NUMBER,
        },
        { /* end of list */ }
    },
};

QemuOptsList qemu_qflex_gen_mem_trace_opts = {
    .name = "qflex-gen-mem_trace",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_qflex_gen_mem_trace_opts.head),
    .desc = {
        { /* end of list */ }
    },
};

#ifdef CONFIG_ARMFLEX
#include "qflex/armflex.h"
QemuOptsList qemu_armflex_opts = {
    .name = "armflex",
    .implied_opt_name = "enable",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_armflex_opts.head),
    .desc = {
        {
            .name = "enable",
            .type = QEMU_OPT_BOOL,
        },
        { /* end of list */ }
    },
};

static void armflex_configure(QemuOpts *opts, Error **errp) {
	bool enable;
    enable = qemu_opt_get_bool(opts, "enable", false);
	armflex_init(false, enable);

}
#endif /* CONFIG_ARMFLEX */

static void qflex_configure(QemuOpts *opts, Error **errp) {
    qflexState.fast_forward = qemu_opt_get_bool(opts, "ff", false);
    qflexState.profile_enable = qemu_opt_get_bool(opts, "profile", false);
    qflexPth.iloop = qemu_opt_get_number(opts, "pth_iloop", 0);

    const char* profile_mode = qemu_opt_get(opts, "profile-mode");
    if(!profile_mode) {
        qflexState.profiling = false;
    } else if (!strcmp(profile_mode, "full")) {
        qflexState.profiling = true;
    } else if (!strcmp(profile_mode, "magic")) {
        qflexState.profiling = false;
    } else {
        error_setg(errp, "QFLEX: Mode specified '%s' is neither full|magic", profile_mode);
    }
}

static void qflex_log_configure(const char *opts) {
    int mask;
    mask = qflex_str_to_log_mask(opts);
    if (!mask) {
        qflex_print_log_usage(opts, stdout);
        exit(1);
    }
    qflex_set_log(mask);
}

static void qflex_gen_mem_trace_configure(QemuOpts *opts, Error **errp) {
	qflex_mem_trace_init();
}

int qflex_parse_opts(int index, const char *optarg, Error **errp) {
#ifdef CONFIG_QFLEX
    QemuOpts *opts;
#endif

    switch(index) {
#ifdef CONFIG_QFLEX
    case QEMU_OPTION_qflex:
        opts = qemu_opts_parse_noisily(
                qemu_find_opts("qflex"), optarg, false);
        if (!opts) { exit(1); }
        qflex_configure(opts, errp);
        qemu_opts_del(opts);
        break;
    case QEMU_OPTION_qflex_d:
        qflex_log_configure(optarg);
        break;
	case QEMU_OPTION_qflex_gen_mem_trace:
		opts = qemu_opts_parse_noisily(
                qemu_find_opts("qflex-gen-mem-trace"), optarg, false);
        if(!opts) { exit(1); }
        qflex_gen_mem_trace_configure(opts, errp);
        qemu_opts_del(opts);
        break;
#endif
#ifdef CONFIG_ARMFLEX
	case QEMU_OPTION_armflex:
		opts = qemu_opts_parse_noisily(qemu_find_opts("armflex"),
											   optarg, false);
		if (!opts) { exit(1); }
		armflex_configure(opts, &error_abort);
        qemu_opts_del(opts);
		break;
#endif /* CONFIG_ARMFLEX */
	default:
		return 0;
    }
    return 1;
}
