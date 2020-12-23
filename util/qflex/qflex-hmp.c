#include "qemu/osdep.h"
#include <dirent.h>
#include "monitor/monitor-internal.h"
#include "qapi/error.h"
#include "qapi/qmp/qdict.h"
#include "qapi/qmp/qnum.h"

#include "qflex/qflex-hmp.h"
#include "qflex/qflex-models.h"

// QFLEX Cache Models functions
void hmp_qflex_mem_trace_start(Monitor *mon, const QDict *qdict) {
    size_t nb_insn = qdict_get_int(qdict, "nb_insn");
    qflex_mem_trace_start(nb_insn);
}

void hmp_qflex_mem_trace_stop(Monitor *mon, const QDict *qdict) {
    qflex_mem_trace_stop();
}

void hmp_qflex_mem_trace_end(Monitor *mon, const QDict *qdict) {
    qflex_mem_trace_end();
}

void hmp_qflex_mem_trace_log_stats(Monitor *mon, const QDict *qdict) {
    char log[256];
    qflex_mem_trace_log_stats(log, 256);
    monitor_printf(mon, "%s", log);
}

#ifdef CONFIG_ARMFLEX
void hmp_armflex_start(Monitor *mon, const QDict *qdict) {
	armflex_start();
}

void hmp_qflex_mem_trace_start(Monitor *mon, const QDict *qdict) {
    size_t nb_insn = qdict_get_int(qdict, "nb_insn");
    armflex_trace_start(nb_insn);
}
#endif
