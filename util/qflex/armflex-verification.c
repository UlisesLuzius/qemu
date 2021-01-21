#include <stdio.h>
#include <stdlib.h>

#include "qflex/qflex.h"
#include "qflex/qflex-models.h"
#include "qflex/qflex-arch.h"
#include "qflex/armflex.h"
#include "qflex/armflex.pb-c.h"
#include "qflex/armflex-communication.h"
#include "qflex/armflex-verification.h"

static bool gen_trace = false;
bool armflex_gen_verification(void) { return gen_trace; }

static bool hasTrace = false;
static int curr_mem = 0;
static size_t inst_count = 0;
static size_t max_inst = 0;

static ArmflexCommitTrace traceState;
static ArmflexCommitTraceP traceProbuf = ARMFLEX_COMMIT_TRACE_P__INIT;
static ArmflexArchStateP stateProbuf = ARMFLEX_ARCH_STATE_P__INIT;
static uint8_t *streamProtobuf;
static size_t streamSize;
static FILE* streamFile;

/* ------ QEMU ------- */
void armflex_gen_verification_start(size_t nb_insn) {
	gen_trace = true;
	hasTrace = false;
	curr_mem = 0;
	inst_count = 0;
	max_inst = nb_insn;
	traceProbuf.state = &stateProbuf;

	armflex_trace_protobuf_open(&traceProbuf, &streamProtobuf, &streamSize);
	armflex_file_stream_open(&streamFile, "ArmflexVerificionBinary");
	armflex_file_stream_write(streamFile, &streamSize, sizeof(streamSize));
	qflex_mem_trace_gen_helper_start();
}


void armflex_gen_verification_end(void) {
	gen_trace = false;
	curr_mem = 0;
	inst_count = 0;
	hasTrace = false;
	gen_trace = false;
	armflex_trace_protobuf_close(&traceProbuf, &streamProtobuf);
	fclose(streamFile);
	qflex_mem_trace_gen_helper_stop();
}

void armflex_gen_verification_add_state(CPUState* cpu, uint64_t addr) {
	if(hasTrace) {
		// When hits next instruction, last instruction is done so 
		// send the full instruction information
		armflex_pack_protobuf_trace(&traceState, &traceProbuf, streamProtobuf);
		armflex_file_stream_write(streamFile, streamProtobuf, streamSize);
		curr_mem = 0;
		if(inst_count >= max_inst) {
			armflex_gen_verification_end();
		}
	}

	armflex_pack_archstate(cpu, &traceState.state);
	uint64_t hva = gva_to_hva(cpu, addr, INST_FETCH);
	uint32_t inst = *((int32_t *) hva);
	traceState.inst = inst;

	hasTrace = true;
	inst_count++;
}

void armflex_gen_verification_add_mem(CPUState* cpu, uint64_t addr) {
	if(curr_mem == 4) { 
		// No more mem insts slots left in CommitTrace struct
		exit(1);
		return; 
	}

	uint64_t hva = gva_to_hva(cpu, addr, DATA_LOAD);
	uint64_t data = *((uint64_t *) hva);
	traceState.mem_addr[curr_mem] = addr;
	traceState.mem_data[curr_mem] = data;

	curr_mem++;
}
