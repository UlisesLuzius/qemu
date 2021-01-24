#ifndef ARMFLEX_VERIFICATION_H
#define ARMFLEX_VERIFICATION_H

#define MAX_MEM_INSTS 8

typedef struct ArmflexCommitTrace {
	ArmflexArchState state;
	uint32_t inst;
	uint64_t mem_addr[MAX_MEM_INSTS];
	uint64_t mem_data[MAX_MEM_INSTS];
} ArmflexCommitTrace;

bool armflex_gen_verification(void);
void armflex_gen_verification_start(size_t nb_insn);
void armflex_gen_verification_end(void);

void armflex_gen_verification_add_state(CPUState* cpu, uint64_t addr);
void armflex_gen_verification_add_mem(CPUState* cpu, uint64_t addr);

#endif
