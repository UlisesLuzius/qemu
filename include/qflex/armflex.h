#ifndef ARMFLEX_H
#define ARMFLEX_H

typedef enum {
    DIRECT, // Runs ARMFLEX from QEMU boot
    MAGIC   // Runs MAGIC when magic inst is hit
} ArmflexModes_t;

typedef struct ArmflexState {
    bool enabled;
    bool running;
    bool gen_sim_trace; // Will generate trace for Chisel to run
} ArmflexState;

typedef struct ArmflexCPU {
	
}

extern ArmflexState armflexState;

static void armflex_init(bool run, bool enabled, bool gen_sim_trace) {
	armflexState.enabled = enabled;
	armflexState.running = run;
	armflexState.gen_sim_trace = gen_sim_trace;
}

static inline void armflex_update_running(bool run) { armflexState.running = run; }
static inline bool armflex_is_running(void) { return armflexState.enabled && armflexState.running; }
static inline bool armflex_in_sim(void) { return armflexState.enabled && armflexState.sim;  }

void armflex_check_memaccess(uint64_t addr, int cpu_index, int type);

/* The following functions are architecture specific, so they are
 * in the architecture target directory.
 * (target/arm/armflex-helper.c)
 */

/* This describes layour of arch state elements
 *
 * XREGS: uint64_t
 * PC   : uint64_t
 * SP   : uint64_t
 * CF/VF/NF/ZF : uint64_t
 */
#define ARCH_PSTATE_NF_MASK     (3)    // 64bit 3
#define ARCH_PSTATE_ZF_MASK     (2)    // 64bit 2
#define ARCH_PSTATE_CF_MASK     (1)    // 64bit 1
#define ARCH_PSTATE_VF_MASK     (0)    // 64bit 0

typedef struct ArmflexArchState {
	uint64_t xregs[32];
	uint64_t pc;
	uint64_t sp;
	uint64_t nzcv;
} ArmflexArchState;

typedef struct ArmflexMemInst {
	int reg;
	uint64_t addr;
	uint64_t data;
}
	
typedef struct ArmflexMemInst {
	int size;
	bool isPair;
	bool isLoad;
	bool is32bit;
	bool isSigned;
	ArmflexMemReq req1;
	ArmflexMemReq req2;

	bool withWriteBack;
	int rd;
	uint64_t res;
} ArmflexMemInst;

typedef struct ArmflexCommitTrace {
	ArmflexArchState* state;
	ArmflexMemInst* mem;
} ArmflexCommitTrace;

extern ArmflexCommitTrace currentCommitInst;

/** Serializes the ARMFLEX architectural state to be transfered with protobuf.
 * @brief armflex_(un)serialize_archstate
 * @param armflex The cpu state
 * @param buffer  The buffer
 * @return        Returns 0 on success
 *
 * NOTE: Don't forget to close the buffer when done
 */
void armflex_serialize_archstate(ArmflexArchState *armflex, void *buffer);
void armflex_unserialize_archstate(void *buffer, ArmflexArchState *armflex);

/** Packs QEMU CPU architectural state into ARMFLEX CPU architectural state.
 * NOTE: Architecture specific: see target/arch/armflex-helper.c
 * @brief armflex_(un)pack_archstate
 * @param cpu     The QEMU CPU state
 * @param armflex The ARMFLEX CPU state
 * @return        Returns 0 on success
 */
void armflex_pack_archstate(CPUState *cpu, ArmflexArchState *armflex);
void armflex_unpack_archstate(ArmflexArchState *armflex, CPUState *cpu);


/**
 * @brief armflex_get_load_addr
 * Translates from guest virtual address to host virtual address
 * NOTE: In case of FAULT, the caller should:
 *          1. Trigger transplant back from FPGA
 *          2. Reexecute instruction
 *          3. Return to FPGA when exception is done
 * @param cpu               Working CPU
 * @param addr              Guest Virtual Address to translate
 * @param acces_type        Access type: LOAD/STORE/INSTR FETCH
 * @param hpaddr            Return Host virtual address associated
 * @return                  uint64_t of value at guest address
 */
bool armflex_get_paddr(CPUState *cpu, uint64_t addr, MMUAccessType access_type,  uint64_t *hpaddr);

/**
 * @brief armflex_get_page Translates from guest virtual address to host virtual address
 * NOTE: In case of FAULT, the caller should:
 *          1. Trigger transplant back from FPGA
 *          2. Reexecute instruction
 *          3. Return to FPGA when exception is done
 * @param cpu               Working CPU
 * @param addr              Guest Virtual Address to translate
 * @param acces_type        Access type: LOAD/STORE/INSTR FETCH
 * @param host_phys_page    Returns Address host virtual page associated
 * @param page_size         Returns page size
 * @return                  If 0: Success, else FAULT was generated
 */
bool armflex_get_ppage(CPUState *cpu, uint64_t addr, MMUAccessType access_type,  uint64_t *host_phys_page, uint64_t *page_size);


/* Messages betweem FPGA/Chisel and QEMU */

#define cmd_base(CMD) {CMD, 0}
typedef enum ArmflexCmds_t {
    // Commands SIM->QEMU
    DATA_LOAD   = 0,
    DATA_STORE  = 1,
    INST_FETCH  = 2,
    INST_UNDEF  = 3,
    INST_EXCP   = 4,

    // Commands QEMU->SIM
    SIM_START = 5, // Load state from QEMU
    SIM_STOP = 6,

    // Commands QEMU<->SIM
    LOCK_WAIT = 7,
    CHECK_N_STEP = 8,
    FA_QFLEXCMDS_NR
} ArmflexCmds_t;

/* short version of ArmflexCmd_t. easy to use in binary files*/
typedef struct ArmflexMsg_t {
    ArmflexCmds_t type;
    uint64_t addr;
} ArmflexMsg_t;

#endif /* ARMFLEX_H */
