#ifndef ARMFLEX_VERIFICATION_H
#define ARMFLEX_VERIFICATION_H

#define MAX_MEM_INSTS (2)
#define ARMFLEX_CACHE_BLOCK_SIZE    (64)

typedef struct ArmflexCommitTrace {
	ArmflexArchState state;
	uint32_t inst;
	uint64_t mem_addr[MAX_MEM_INSTS];
	uint64_t mem_data[MAX_MEM_INSTS];
	uint8_t inst_block_data[ARMFLEX_CACHE_BLOCK_SIZE];
	uint8_t mem_block_data[MAX_MEM_INSTS][ARMFLEX_CACHE_BLOCK_SIZE];
} ArmflexCommitTrace;

bool armflex_gen_verification(void);
void armflex_gen_verification_start(size_t nb_insn);
void armflex_gen_verification_end(void);

void armflex_gen_verification_inst_cancelled(void);

void armflex_gen_verification_add_state(CPUState* cpu, uint64_t addr);
void armflex_gen_verification_add_mem(CPUState* cpu, uint64_t addr);

#endif
