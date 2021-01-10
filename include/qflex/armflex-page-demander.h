#ifndef ARMFLEX_PAGE_DEMANDER_H
#define ARMFLEX_PAGE_DEMANDER_H

typedef IPTHvpPtr* IPTHvpPtrPtr;
typedef IPTHvp*    IPTHvpPtr;

typedef struct InvertedPageTable {
	IPTHvpPtr* entries;
} InvertedPageTable;

typedef struct IPTHvp {
	int        cnt;   // Count of elements in IPTGvpList
	uint64_t   hvp;   // Host Virtual Address (HVP)
	IPTGvpList *head; // GVP mapping to this HVP present in the FPGA
	IPTHvp     *next; // In case multiple Host VA hash to same spot
} IPTHvp;

/* 
 * Layout of Guest VA (GVA)
 * GVA |  isKernel(K)  | PAGE_NUMBER | PAGE_OFFST |
 * bits| 63         48 | 47       12 | 11       0 |
 * val | 0xFFFF/0x0000 |  Any Number | Don't Care |
 *
 * NOTE: PID in linux is usually from 0 to 0x0FFF
 *
 * Compress Guest VA, PID, Permission (P)
 *     | K  |   PID   | PAGE_NUMBER | Don't Care |  P  |
 * bits| 63 | 62   48 | 47       12 | 11      10 | 1 0 |
 */

#define IPT_MASK_PID            (0x7FFF << 48)
#define IPT_MASK_PAGE_NB        (0XFFFFFFFFF << 12)
#define IPT_MASK_isKernel       (1 << 63)
#define IPT_MASK_P              (0b11)
#define PAGE_MASK               (0xFFFF)

#define IPT_GET_PID(bits)         (bits >> 48)
#define IPT_GET_VA_BITS(bits)     (bits & IPT_MASK_PAGE_NB)
#define IPT_GET_KERNEL_BITS(bits) ((bits & IPT_MASK_isKernel) ? (0xFFFF) << 48 : 0x0)
#define IPT_GET_PER(bits)         (bits & IPT_MASK_P)
#define IPT_GET_VA(bits)          (IPT_GET_KERNEL_BITS(bits) | IPT_GET_VA_BITS(bits))
#define IPT_GET_CMP(bits)         (bits & ~PAGE_MASK) // Drop Permission

#define IPT_SET_KERNEL(va)  (va & (1ULL << 63))
#define IPT_SET_PID(pid)    (pid << 48)
#define IPT_SET_VA_BITS(va) (va & IPT_MASK_PAGE_NB)
#define IPT_SET_PER(p)      (p  & IPT_MASK_P)
#define IPT_COMPRESS(va, pid, p) \
	(IPT_SET_KERNEL(va) | IPT_SET_PID(pid) | IPT_SET_VA_BITS(va) | IPT_SET_PER(p))

#define GET_PAGE(bits)       (bits & ~0xFFF)

typedef struct IPTHvpList {
	uint64_t ipt_bits;    // PID, Guest VA, Permission
	IPTHvpList *next; // Extra synonyms list
} IPTHvpList;

/* Call this function before a tcg_gen_qemu_ld/st is executed
 * to make sure that QEMU has the latest page modifications
 * NOTE: Uses 'qflex_mem_trace' helper as trigger which is 
 * generated before memory instructions
 */ 
void armflex_synchronize_page(CPUState *cpu, uint64_t vaddr, int type);

#endif
