#ifndef ARMFLEX_VERIFICATION_H
#define ARMFLEX_VERIFICATION_H

typedef struct ArmflexCommitTrace {
	ArmflexArchState state;
	uint32_t inst;
	uint64_t mem_addr[4];
	uint64_t mem_data[4];
} ArmflexCommitTrace;

bool armflex_gen_verification(void);
void armflex_gen_verification_start(size_t nb_insn);
void armflex_gen_verification_end(void);

void armflex_gen_verification_add_state(CPUState* cpu, uint64_t addr);
void armflex_gen_verification_add_mem(CPUState* cpu, uint64_t addr);

#endif
