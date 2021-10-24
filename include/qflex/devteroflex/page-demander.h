#ifndef DEVTEROFLEX_PAGE_DEMANDER_H
#define DEVTEROFLEX_PAGE_DEMANDER_H

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

#define IPT_MASK_PID        (0x7fffULL << 48)
#define IPT_MASK_PAGE_NB    (0xfffffffffULL << 12)
#define IPT_MASK_isKernel   (1ULL << 63)
#define IPT_MASK_P          (0b11ULL)
#define PAGE_MASK           (0xffffULL)

#define IPT_GET_PID(bits)           (bits >> 48)
#define IPT_GET_VA_BITS(bits)       (bits & IPT_MASK_PAGE_NB)
#define IPT_GET_KERNEL_BITS(bits)   ((bits & IPT_MASK_isKernel) ? (0xffffULL << 48) : 0x0)
#define IPT_GET_PER(bits)           (bits & IPT_MASK_P)
#define IPT_GET_VA(bits)            (IPT_GET_KERNEL_BITS(bits) | IPT_GET_VA_BITS(bits))
#define IPT_GET_CMP(bits)           (bits & ~PAGE_MASK) // Drop Permission
#define IPT_ASSEMBLE_64(hi, lo)     ((uint64_t) hi << 32 | (uint64_t) lo)

#define IPT_SET_KERNEL(va)  (va & (1ULL << 63))
#define IPT_SET_PID(pid)    ((unsigned long long) pid << 48)
#define IPT_SET_VA_BITS(va) (va & IPT_MASK_PAGE_NB)
#define IPT_SET_PER(p)      (p  & IPT_MASK_P)
#define IPT_COMPRESS(va, pid, p) \
    (IPT_SET_KERNEL(va) | IPT_SET_PID(pid) | IPT_SET_VA_BITS(va) | IPT_SET_PER(p))

#define GET_PAGE(bits)      (bits & ~0xfffULL)

typedef struct IPTGvpList {
    uint64_t ipt_bits;    // PID, Guest VA, Permission
    struct IPTGvpList *next; // Extra synonyms list
} IPTGvpList;

typedef struct IPTHvp {
    int           cnt;   // Count of elements in IPTGvpList
    uint64_t      hvp;   // Host Virtual Address (HVP)
    IPTGvpList    *head; // GVP mapping to this HVP present in the FPGA
    struct IPTHvp *next; // In case multiple Host VA hash to same spot
} IPTHvp;

typedef IPTHvp *    IPTHvpPtr;
typedef IPTHvpPtr * IPTHvpPtrPtr;

typedef struct InvertedPageTable {
    IPTHvpPtr *entries;
} InvertedPageTable;

typedef enum PageTypes {
    PAGE = 0,
    SYNONYM = 1
} PageTypes;

/* Call this function when the devteroflex core has evicted an entry
 */
int devteroflex_ipt_evict(uint64_t hvp, uint64_t ipt_bits);
    
/* Call this function when the devteroflex core requests a new page
 */
int devteroflex_ipt_add_entry(uint64_t hvp, uint64_t ipt_bits, uint64_t *synonyms);
    
/* Call this function before a tcg_gen_qemu_ld/st is executed
 * to make sure that QEMU has the latest page modifications
 * NOTE: Uses 'qflex_mem_trace' helper as trigger which is 
 * generated before memory instructions
 */ 
void devteroflex_synchronize_page(CPUState *cpu, uint64_t vaddr, int type);

/*
 *
 */
void devteroflex_push_page_fault(uint64_t hvp, uint64_t ipt_bits);
 
/* Get a free physical addr for a data page
 */
int get_free_paddr(void);

/* Operations to align while waiting for lazy evictions
 */
void push_raw(uint64_t hvp, uint64_t ipt_bits);
void run_pending_raw(uint64_t hvp);
bool has_raw(uint64_t hvp);
 
#endif
