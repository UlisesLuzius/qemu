#ifndef ARMFLEX_H
#define ARMFLEX_H

#include <stdbool.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#define PAGE_SIZE 4096

// See cpu.h to match MMUAccessType
typedef enum MemoryAccessType {
    DATA_LOAD  = 0,
    DATA_STORE = 1,
    INST_FETCH = 2
} MemoryAccessType;

typedef struct ArmflexConfig {
    bool enabled;
    bool running;
} ArmflexConfig;

extern ArmflexConfig armflexConfig;

static inline void armflex_init(bool run, bool enabled) {
	armflexConfig.enabled = enabled;
	armflexConfig.running = run;
}
static inline void armflex_update_running(bool run) { armflexConfig.running = run; }
static inline bool armflex_is_running(void) { return armflexConfig.enabled && armflexConfig.running; }

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
#define ARMFLEX_TOT_REGS        (35)

typedef struct ArmflexArchState {
	uint64_t xregs[32];
	uint64_t pc;
	uint64_t sp;
	uint64_t nzcv;
} ArmflexArchState;

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
void armflex_pack_archstate(ArmflexArchState *armflex, CPUState *cpu);
void armflex_unpack_archstate(CPUState *cpu, ArmflexArchState *armflex);

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
bool armflex_get_paddr(CPUState *cpu, uint64_t addr, uint64_t access_type,  uint64_t *hpaddr);

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
bool armflex_get_ppage(CPUState *cpu, uint64_t addr, uint64_t access_type,  uint64_t *host_phys_page, uint64_t *page_size);

int armflex_execution_flow(void);

#endif /* ARMFLEX_H */