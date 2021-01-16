#include <stdio.h>
#include <stdlib.h>

#include "qflex/qflex-arch.h"
#include "qflex/armflex.h"
#include "qflex/armflex.pb-c.h"
#include "qflex/armflex-communication.h"
#include "qflex/armflex-verification.h"

/* ----- PROTOBUF ------ */
static void armflex_trace_protobuf_open(ArmflexCommitTraceP *traceP,
							 void **stream, size_t *size) {
	// Init fields
	traceP->n_mem_addr = 4;
	traceP->mem_addr = malloc (sizeof (uint64_t) * 4);
	traceP->n_mem_data = 4;
	traceP->mem_data = malloc (sizeof (uint64_t) * 4);
	assert(traceP->mem_addr);
	assert(traceP->mem_data);

	// Init State
	traceP->state = malloc (sizeof (ArmflexArchStateP));
	ArmflexArchStateP *state = traceP->state;
	state->n_xregs = 32;
	state->xregs = malloc (sizeof (uint64_t) * 32);
	assert(state->xregs);

	*size = armflex_commit_trace_p__get_packed_size (traceP); // This is calculated packing length
	*stream = malloc(*size);                               // Allocate required serialized buffer length 
	assert(stream);
}

static void armflex_trace_protobuf_close(ArmflexCommitTraceP *traceP,
							  void **stream) {
	// Free allocated fields
	free(traceP->mem_addr);
	free(traceP->mem_data);
	free(*stream);
}

static void armflex_pack_protobuf_trace(ArmflexCommitTrace *trace,
										ArmflexCommitTraceP *traceP, 
										void **stream) {
	// Pack Commit Trace
	traceP->inst = trace->inst;
	memcpy(&traceP->mem_addr, &trace->mem_addr, sizeof(uint64_t) * traceP->n_mem_addr);
	memcpy(&traceP->mem_data, &trace->mem_data, sizeof(uint64_t) * traceP->n_mem_data);

	// Pack Armflex State
	ArmflexArchState *state = &trace->state;
	ArmflexArchStateP *stateP = traceP->state;
	memcpy(&stateP->xregs, &state->xregs, sizeof(uint64_t) * stateP->n_xregs);
	stateP->pc = state->pc;
	stateP->sp = state->sp;
	stateP->nzcv = state->nzcv;

	// protobuf struct -> protobuf stream
	armflex_commit_trace_p__pack (traceP, *stream); // Pack the data
}

/* ------ QEMU ------- */
static bool gen_trace = false;
bool armflex_gen_verification(void) { return gen_trace; }

static bool hasTrace = false;
static int curr_mem = 0;
static size_t inst_count = 0;
static size_t max_inst = 0;

static ArmflexCommitTrace trace;
static ArmflexCommitTraceP traceP;
static void *stream;
static size_t size;

void armflex_gen_verification_start(size_t nb_insn) {
	gen_trace = true;
	hasTrace = false;
	curr_mem = 0;
	inst_count = 0;
	max_inst = nb_insn;

	armflex_trace_protobuf_open(&traceP, &stream, &size);
	// armflex_write_protobuf_base_size(size);
}


void armflex_verification_close(void) {
	gen_trace = false;
	curr_mem = 0;
	inst_count = 0;
	hasTrace = false;
	gen_trace = false;
	armflex_trace_protobuf_close(&traceP, &stream);
}

void armflex_verification_gen_state(CPUState* cpu, uint64_t addr) {
	if(hasTrace) {
		// When hits next instruction, last instruction is done so 
		// send the full instruction information
		armflex_pack_protobuf_trace(&trace, &traceP, &stream);
		// armflex_write_protobuf(stream);
		curr_mem = 0;
		if(inst_count >= max_inst) {
			armflex_verification_close();
		}
	}

	armflex_pack_archstate(cpu, &trace.state);
	uint64_t hva = gva_to_hva(cpu, addr, INST_FETCH);
	uint32_t inst = *((int32_t *) hva);
	trace.inst = inst;

	hasTrace = true;
	inst_count++;
}

void armflex_verification_add_mem(CPUState* cpu, uint64_t addr) {
	if(curr_mem == 4) { 
		// No more mem insts slots left in CommitTrace struct
		exit(1);
		return; 
	}

	uint64_t hva = gva_to_hva(cpu, addr, DATA_LOAD);
	uint64_t data = *((uint64_t *) hva);
	trace.mem_addr[curr_mem] = addr;
	trace.mem_data[curr_mem] = data;

	curr_mem++;
}
