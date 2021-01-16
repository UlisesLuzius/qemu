#include <stdbool.h>
#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qflex/qflex.h"
#include "qflex/armflex.h"
#include "qflex/armflex-page-demander.h"

static InvertedPageTable ipt;

static void IPTGvpList_free(IPTGvpList **base) {
	IPTGvpList *curr = *base;
	while(curr) {
		free(curr);
		curr = curr->next;
	};
}

static void IPTHvp_evict(IPTHvp **entry, IPTHvpPtr *entryPtr) {
	IPTGvpList_free(&((*entry)->head));
	*entryPtr = (*entry)->next;
	free(*entry);
}

static void IPTGvpList_insert(IPTHvp *entry, uint64_t ipt_bits, uint64_t *chain) {
	IPTGvpList **ptr = &entry->head;
	int cnt = 0;
	while(*ptr) {
		if(chain) { chain[cnt] = (*ptr)->ipt_bits; cnt++; }
		ptr = &(*ptr)->next;	
	}
	*ptr = malloc(sizeof(IPTGvpList));
	(*ptr)->ipt_bits = ipt_bits;
}

static void IPTGvpList_del(IPTHvp *base, uint64_t ipt_bits) {
	IPTGvpList **ptr = &base->head;
	IPTGvpList *ele = base->head;
	IPTGvpList *new_next;

	while(ele != NULL && IPT_GET_CMP(ele->ipt_bits) != IPT_GET_CMP(ipt_bits)) {
		ptr = &ele->next;
		ele = ele->next;
	}

	if(ele) {
		// Found matching entry
		new_next = ele->next; // Save ptr to next element in linked-list
		free(*ptr);           // Delete entry
		*ptr = new_next;      // Linked-list skips deleted element
	}
}

#define NB_ENTRIES (1 << 10)
static inline size_t IPT_hash(uint64_t hvp) {
	return (hvp % NB_ENTRIES);
}

/**
 * IPTHvp_get: Get IPT entry from the HVP
 *
 * Returns 'entry' which points to the IPT entry and 
 * 'entryPtr' which stores the ptr that points to 'entry' location
 *
 * If *entry is NULL then the HVP is not present in the FPGA
 *
 * @hvp: Host Virtual Page of IPT entry to find
 * @entry:    ret pointer to the matching hvp entry 
 * @entryPtr: ret pointer to the 'entry' ptr
 */
static void IPTHvp_get(uint64_t hvp, IPTHvp **entry, IPTHvpPtr **entryPtr) {
	size_t entry_idx = IPT_hash(hvp);
	*entry = ipt.entries[entry_idx];
	*entryPtr = &ipt.entries[entry_idx];
	while((*entry) != NULL && (*entry)->hvp != hvp) {
		*entryPtr = &((*entry)->next);
		*entry = (*entry)->next;
	}
}

static void IPTHvp_insert_Gvp(uint64_t hvp, uint64_t ipt_bits, uint64_t **chain, size_t *cnt) {
	IPTHvp *entry;
	IPTHvpPtr *entryPtr;

	IPTHvp_get(hvp, &entry, &entryPtr);
	if(entry) {
		// Has synonyms, return synonym list
		*cnt = entry->cnt;
		*chain = calloc(entry->cnt, sizeof(uint64_t));
	} else {
		// Create HVP entry if not present
		*entryPtr = malloc(sizeof(IPTHvp));
		entry = *entryPtr;
		entry->hvp = hvp;
		entry->cnt = 0;
	}

	if(ipt_bits) {
		IPTGvpList_insert(entry, ipt_bits, *chain);
		entry->cnt++;
	}
}

static void IPTHvp_evict_Gvp(uint64_t hvp, uint64_t ipt_bits) {
	IPTHvp *entry;
	IPTHvpPtr *entryPtr;
	
	IPTHvp_get(hvp, &entry, &entryPtr);
	IPTGvpList_del(entry, ipt_bits);
	entry->cnt--;
	if(entry->head == NULL) {
		// There's no more gvp mapped to this hvp after eviction
		IPTHvp_evict(&entry, entryPtr);
	}
}

int armflex_get_page(CPUState *cpu, uint64_t vaddr, int type) {
	uint64_t hvp = gva_to_hva(cpu, vaddr & PAGE_MASK, type) ;
	int pid = QFLEX_GET_ARCH(pid)(cpu);
	uint64_t ipt_bits = IPT_COMPRESS(vaddr, pid, type);
	if(hvp == -1) {
		// Page Fault/Permission Violation
		// armflex_push_resp(cpu, ipt_bits, FAULT); TODO
	} else {
		// 1. Add entry to IPT
		uint64_t *chain = NULL; // Synonyms
		size_t cnt = 0;
		IPTHvp_insert_Gvp(hvp, ipt_bits, &chain, &cnt);

		// 2. Pack response to FPGA
		if(chain) {
			// 2a. Synonyms
			// armflex_push_resp(cpu, ipt_bits, SYNONYM); TODO
			// armflex_push_synonyms(cpu, chain, cnt); TODO
		} else {
			// 2b. send 4K PAGE to FPGA
			// armflex_push_resp(cpu, ipt_bits, PAGE); TODO
			// armflex_push_page(cpu, hvp); TODO
		}
		free(chain);
	}
	// Send response to FPGA
	// armflex_send_message(cpu); TODO
    return 0;
}

void armflex_synchronize_page(CPUState *cpu, uint64_t vaddr, int type) {
	uint64_t hvp = gva_to_hva(cpu, vaddr, type) & ~PAGE_MASK;
	if(hvp == -1) {
		// vaddr not mapped, no need to synchronize
		return;
	}

	uint64_t *evicts;
	IPTHvp *entry; 
	IPTHvpPtr *entryPtr; // Used to fast IPT eviction if synchronization necessary

	IPTHvp_get(hvp, &entry, &entryPtr);
	
	if(entry) {
		// Page is present in FPGA

		// Synchronize if it's a store
		bool synchronize = (type == DATA_STORE);

		// Check whenever one of the process has write permissions on it
		// Implies FPGA can have modified data and requires synchronization
		evicts = calloc(entry->cnt, sizeof(uint64_t));
		int cnt;
		IPTGvpList *gvpEntry;
		for(gvpEntry = entry->head, cnt = 0; 
			gvpEntry != NULL; gvpEntry = gvpEntry->next, cnt++) {
			if(IPT_GET_PER(gvpEntry->ipt_bits) == DATA_STORE) {
				synchronize = true;
			}
			evicts[cnt] = gvpEntry->ipt_bits;
		}

		if(synchronize) {
			// Either a store or the FPGA had write permissions
			// armflex_push_resp(cpu, EVICT); TODO
			// armflex_evict_list(evicts); TODO
			// Wait for complete
			// wait_fpga_ack(); TODO
			// Evict entry from the IPT
			IPTHvp_evict(&entry, entryPtr);
		}
		free(evicts);
	}
}
