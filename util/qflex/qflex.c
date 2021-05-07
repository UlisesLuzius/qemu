#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qapi/error.h"
#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/main-loop.h"

#include "qflex/qflex.h"
#include "qflex/qflex-profiling.h"
#include "qflex/qflex-traces.h"

qflex_state_t qflexState;
qflex_pth_t qflexPth;

void qflex_api_values_init(CPUState *cpu) {
    qflexState.inst_done = false;
    qflexState.broke_loop = false;
    qflexState.prologue_done = false;
    qflexState.prologue_pc = QFLEX_GET_ARCH(pc)(cpu);
    qflexState.exec_type = QEMU;
    qflexState.profile_enable = false;
    qflexState.profiling = false;
    qflexState.fast_forward = false;

    qflexPth.iloop = 0;
    qflexPth.iexit = 0;

    // qflex_model_init();
}

int qflex_prologue(CPUState *cpu) {
    int ret = 0;
    qflex_log_mask(QFLEX_LOG_GENERAL, "QFLEX: PROLOGUE START:%08"PRIx64"\n"
                   "    -> Skips initial snapshot load long interrupt routine to normal user program\n", QFLEX_GET_ARCH(pc)(cpu));
    qflex_update_exec_type(PROLOGUE);
    while(!qflex_is_prologue_done()) {
        ret = qflex_cpu_step(cpu);
    }
    qflex_log_mask(QFLEX_LOG_GENERAL, "QFLEX: PROLOGUE END  :%08"PRIx64"\n", QFLEX_GET_ARCH(pc)(cpu));
    qflex_update_inst_done(false);
    return ret;
}

int qflex_singlestep(CPUState *cpu) {
    int ret = 0;
    qflex_update_exec_type(SINGLESTEP);

    while(!qflex_is_inst_done()) {
        ret = qflex_cpu_step(cpu);
    }

    qflex_update_inst_done(false);

    return ret;
}

int qflex_exception(CPUState *cpu) {
    int ret = 0;
    qflex_update_exec_type(EXCEPTION);
    while(QFLEX_GET_ARCH(el)(cpu) != 0) {
        ret = qflex_cpu_step(cpu);
    }
    qflex_update_inst_done(false);
    return ret;
}

int qflex_profile(CPUState *cpu) {
    int ret = 0;
    int last_el = QFLEX_GET_ARCH(el)(cpu);
    int curr_el = QFLEX_GET_ARCH(el)(cpu);
    FILE *logfile;

    qflex_update_exec_type(SINGLESTEP);

    qflex_log_mask(QFLEX_LOG_PROFILE_EL, "PROFILE: START EL%i\n", curr_el);
    qflex_profile_log_l2h_names_csv();
    while(qflex_is_profiling()) {
        last_el = QFLEX_GET_ARCH(el)(cpu);
        ret = qflex_cpu_step(cpu);
        curr_el = QFLEX_GET_ARCH(el)(cpu);
        if(curr_el != last_el) {
            if(unlikely(qflex_loglevel_mask(QFLEX_LOG_PROFILE_EL))) {
                if(1) {
                    logfile = qemu_log_lock();
                    qflex_profile_curr_el_log_stats_csv(last_el);
                    qemu_log_unlock(logfile);
                } else {
                    // Verbose
                    logfile = qemu_log_lock();
                    qflex_profile_curr_el_log_stats_verbose();
                    qemu_log("PROFILE: EL%i -> EL%i\n", last_el, curr_el);
                    qemu_log_unlock(logfile);
                }
            }
            qflex_profile_curr_el_reset();
        }
    }
    if(qflex_loglevel_mask(QFLEX_LOG_GENERAL)) {
        logfile = qemu_log_lock();
        qflex_profile_curr_el_log_stats_csv(QFLEX_GET_ARCH(el)(cpu));
        qemu_log("PROFILE: STATS END");
        qflex_profile_global_log_stats();
        qemu_log_unlock(logfile);
    }
    qflex_profile_global_reset();
    qflex_update_inst_done(false);
    return ret;
}

int qflex_adaptative_execution(CPUState *cpu) {
    while(1) {
        if(qflex_is_profiling()) {
            qflex_profile(cpu);
        } else {
            qflex_singlestep(cpu);
        }
    }
}