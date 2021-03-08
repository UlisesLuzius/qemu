#include <stdio.h>
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
#include "qflex/armflex-communication.h"

// ------ CHECK --------



// ------ TRACE --------
static size_t total_insts = 0;
static size_t total_mem = 0;
static size_t total_ld = 0;
static size_t total_st = 0;
static size_t total_trace_insts = 0;
static bool gen_helper= false;
static bool gen_trace = false;
static bool gen_inst = false;

static FILE** traceFiles;
static FILE** instFiles;

void qflex_mem_trace_init(int core_count) {
	total_insts = 0;
    total_mem = 0;
	gen_helper = false;
	gen_trace = false;
	traceFiles = calloc(core_count, sizeof(FILE*));

	char filename[sizeof "mem_trace_00"];
	for(int i = 0; i < core_count; i++) {
		if(i > 64) exit(1);
        sprintf(filename, "mem_trace_%02d", i);
		qemu_log("Filename %s\n", filename);
	    armflex_file_stream_open(&traceFiles[i], filename);
	}

// Instruction trace
	instFiles = calloc(core_count, sizeof(FILE*));
	gen_inst = false;

	char filenameInst[sizeof "inst_trace_00"];
	for(int i = 0; i < core_count; i++) {
 		if(i > 64) exit(1);
        sprintf(filenameInst, "inst_trace_%02d", i);
		qemu_log("Filename %s\n", filename);
	    armflex_file_stream_open(&instFiles[i], filenameInst);
	}
}

void qflex_inst_trace(uint32_t inst, uint64_t pid) {
	armflex_file_stream_write(instFiles[pid], &inst, sizeof(inst));
}

typedef struct mem_trace_data {
	uint32_t type;
	uint64_t addr;
	uint64_t hwaddr;
} mem_trace_data;

void qflex_mem_trace_memaccess(uint64_t addr, uint64_t hwaddr, uint64_t pid, uint64_t type) {
	//FILE *logfile = qemu_log_lock();
	//qemu_log("CPU%"PRIu64":%"PRIu64":0x%016"PRIx64":0x%016"PRIx64"\n", 
	//		 pid, type, addr, hwaddr);
	//qemu_log_unlock(logfile);
	mem_trace_data trace = {.addr = addr, .hwaddr = hwaddr, .type = type};
	armflex_file_stream_write(traceFiles[pid], &trace, sizeof(trace));

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


void qflex_mem_trace_gen_helper_start(void) { 
	gen_helper = true; 
	qflex_tb_flush();
}

void qflex_mem_trace_gen_helper_stop(void) {
	gen_helper = false;
	qflex_tb_flush();
}

void qflex_mem_trace_start(size_t nb_insn) { 
	total_trace_insts = nb_insn;
	gen_helper = true; 
	gen_trace = true;
	gen_inst = true;
	qflex_tb_flush();
}

void qflex_mem_trace_stop(void) {
	gen_helper = false;
	gen_trace = false;
	qflex_tb_flush();
}

void qflex_mem_trace_end(void) { 
	total_insts = 0;
    total_mem = 0;
	gen_helper = false;
	gen_trace = false;
    qflex_tb_flush();
	//for(int i = 0; i < core_count; i++) {
	//	fclose(traceFiles[i]);
	//}
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
