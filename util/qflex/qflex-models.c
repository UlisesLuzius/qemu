#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qemu-common.h"
#include "qemu/error-report.h"
#include "qapi/qmp/qerror.h"
#include "qemu/option_int.h"
#include "qemu/main-loop.h"

#include "hw/core/cpu.h"

#include "qflex/qflex.h"
#include "qflex/qflex-log.h"
#include "qflex/qflex-models.h"

// ------ CHECK --------



// ------ TRACE --------
static size_t total_insts = 0;
static size_t total_mem = 0;
static size_t total_ld = 0;
static size_t total_st = 0;
static size_t total_trace_insts = 0;
static bool gen_helper= false;
static bool gen_trace = false;

void qflex_mem_trace_init(void) {
	total_insts = 0;
    total_mem = 0;
	gen_helper = false;
	gen_trace = false;
}

void qflex_mem_trace_memaccess(uint64_t addr, uint64_t hwaddr, uint64_t pid, uint64_t type) {
	FILE *logfile = qemu_log_lock();
	qemu_log("CPU%llu:%llu:0x%016llx:0x%016llx\n", pid, type, addr, hwaddr);
	qemu_log_unlock(logfile);

	switch(type) {
		case MMU_DATA_LOAD: total_ld++; total_mem++; break;
		case MMU_DATA_STORE: total_st++; total_mem++; break;
		case MMU_INST_FETCH: total_insts++; break;
	}

	if(total_insts >= total_trace_insts) {
		qflex_mem_trace_log_direct();
		qflex_mem_trace_end();
		printf("QFLEX: mem Trace DONE\n");
	}
}

void qflex_mem_trace_start(size_t nb_insn) { 
	total_trace_insts = nb_insn;
	gen_helper = true; 
	gen_trace = true;
	qflex_tb_flush();
}

void qflex_mem_trace_stop(void) {
	gen_helper = false;
	gen_trace = false;
	qflex_tb_flush();
}

void qflex_mem_trace_end(void) { 
	qflex_mem_trace_init();
    qflex_tb_flush();
}

bool qflex_mem_trace_gen_helper(void) { return gen_helper; }
bool qflex_mem_trace_gen_trace(void) { return gen_trace; }

void qflex_mem_trace_log_stats(char *buffer, size_t max_size) { 
	size_t tot_chars;
	tot_chars = snprintf(buffer, max_size,
	    "Memory System statistics:\n"
	    "  TOT Fetch Insts: %zu\n"
	    "  TOT LD/ST Insnts: %zu\n",
	    total_insts, total_mem);

    assert(tot_chars < max_size);
}

void qflex_mem_trace_log_direct(void) {
   char str[256];
   qflex_mem_trace_log_stats(str, 256);
   qemu_log("%s", str);
}

// ------ END TRACE --------
