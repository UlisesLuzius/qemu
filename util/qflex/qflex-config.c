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

#ifdef CONFIG_EXTSNAP
#include "migration/savevm-ext.h"
#endif

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

#ifdef CONFIG_EXTSNAP
QemuOptsList qemu_phases_opts = {
    .name = "phases",
    .implied_opt_name = "steps",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_phases_opts.head),
    .desc = {
        {
            .name = "steps",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "name",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};

QemuOptsList qemu_ckpt_opts = {
    .name = "ckpt",
    .implied_opt_name = "every",
    .merge_lists = true,
    .head = QTAILQ_HEAD_INITIALIZER(qemu_ckpt_opts.head),
    .desc = {
        {
            .name = "every",
            .type = QEMU_OPT_STRING,
        }, {
            .name = "end",
            .type = QEMU_OPT_STRING,
        },
        { /* end of list */ }
    },
};
#endif

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
#if defined(CONFIG_EXTSNAP) && defined(CONFIG_FLEXUS)
	case QEMU_OPTION_phases:
		if (is_ckpt_enabled())
			fprintf(stderr, "cant use phases and ckpt together");
		opts = qemu_opts_parse_noisily(qemu_find_opts("phases"),
											  optarg, true);
		if (!opts) {
			exit(1);
		}
		toggle_phases_creation();
		configure_phases(opts, errp);
		qemu_opts_del(opts);
		break;

	case QEMU_OPTION_ckpt:
		if (is_phases_enabled())
			fprintf(stderr, "cant use phase and ckpt together");
		opts = qemu_opts_parse_noisily(qemu_find_opts("ckpt"),
											optarg, true);
		if (!opts) {
			exit(1);
		}
		toggle_ckpt_creation();
		configure_ckpt(opts, errp);
		qemu_opts_del(opts);
		break;
#endif
    default:
        return 0;
    }
    return 1;
}


void processLetterforExponent(uint64_t *val, char c, Error **errp)
{
    switch(c) {
        case 'K': case 'k' :
        *val *= KIL;
        break;
        case 'M':case 'm':
        *val  *= MIL;
        break;
        case 'B':case 'b':
        *val  *= BIL;
        break;
        default:
		error_setg(errp, "the suffix you used is not valid:\n"
				         "   valid suffixes are K,k,M,m,B,b");
        exit(1);
        break;
    }
}

void processForOpts(uint64_t *val, const char* qopt, Error **errp)
{
    size_t s = strlen(qopt);
    char c = qopt[s-1];

    if (isalpha(c)) {
        char* temp= strndup(qopt,  strlen(qopt)-1);
        *val = atoi(temp);
        free(temp);
        if (*val <= 0){
            *val = 0;
            return;
        }

        processLetterforExponent(&(*val), c, errp);
    } else {
        *val = atoi(qopt);
    }
}

#if defined(CONFIG_EXTSNAP) && defined(CONFIG_FLEXUS)
void configure_phases(QemuOpts *opts, Error **errp) {
    const char* step_opt, *name_opt;
    step_opt = qemu_opt_get(opts, "steps");
    name_opt = qemu_opt_get(opts, "name");

    int id = 0;
    if (!step_opt) {
        error_setg(errp, "no distances for phases defined");
    }
    if (!name_opt) {
        fprintf(stderr, "no naming prefix  given for phases option. will use prefix phase_00X");
        phases_prefix = strdup("phase");
    } else {
        phases_prefix = strdup(name_opt);
    }

    phases_state_t * head = calloc(1, sizeof(phases_state_t));

    char* token = strtok((char*) step_opt, ":");
    processForOpts(&head->val, token, errp);
    head->id = id++;
    QLIST_INSERT_HEAD(&phases_head, head, next);

    while (token) {
        token = strtok(NULL, ":");

        if (token) {
            phases_state_t* phase = calloc(1, sizeof(phases_state_t));
            processForOpts(&phase->val, token, errp);
            phase->id= id++;
            QLIST_INSERT_AFTER(head, phase, next);
            head = phase;
        }
    }
}

void configure_ckpt(QemuOpts *opts, Error **errp) {
    const char* every_opt, *end_opt;
    every_opt = qemu_opt_get(opts, "every");
    end_opt = qemu_opt_get(opts, "end");

    if (!every_opt) {
        error_setg(errp, "no interval given for ckpt option. cant continue");
    }
    if (!end_opt) {
        error_setg(errp, "no end given for ckpt option. cant continue");
    }

    processForOpts(&ckpt_state.ckpt_interval, every_opt, errp);
    processForOpts(&ckpt_state.ckpt_end, end_opt, errp);

    if (ckpt_state.ckpt_end < ckpt_state.ckpt_interval) {
        error_setg(errp, "ckpt end cant be smaller than ckpt interval");
    }
}

#endif // CONFIG_EXTSNAP


