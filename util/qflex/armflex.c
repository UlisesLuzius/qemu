#include "qflex/armflex.h"

static size_t inst_count = 0;

void armflex_hit_inst() {

}

void armflex_trace_start(size_t nb_insn) { 
	inst_count = nb_insn;
	armflex.gen_sim_trace = true; 
	armflex.running = true;
	qflex_tb_flush();
}
