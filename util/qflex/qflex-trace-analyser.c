#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

typedef enum {
  ID_ICache,
  ID_DCache,
  ID_ITLB,
  ID_DTLB,
  ID_TLB,
  ID_UNDEF
} QFlexCache_t;

typedef struct ValStr_t {
	int val;
	const char* str;
} ValStr_t;

const ValStr_t get_str[ID_UNDEF] = {
	{ID_ICache,  "ID_ICache"},
  	{ID_DCache,  "ID_DCache"},
  	{ID_ITLB,    "ID_ITLB  "}, 
  	{ID_DTLB,    "ID_DTLB  "}, 
  	{ID_TLB,     "ID_TLB   "}
};


typedef struct cache_entry_t {
	bool valid; // Valid Bitmap
	int pid;
    size_t tag;
    size_t time;
} cache_entry_t;


typedef struct cache_model_t {
    size_t block_size; // argument in msb
    size_t sets;
    size_t associativity;
    size_t nb_entries; // sets*associativity
    size_t curr_time;
    size_t misses;

	// Entries
	cache_entry_t* entries;
} cache_model_t;

typedef struct mem_access_t {
	bool isData;
	bool isStore;
	int pid;
	size_t vaddr;
	size_t paddr;
} mem_access_t;

static size_t total_insts = 0;
static size_t total_mem = 0;
static cache_model_t iCache;
static cache_model_t dCache;
static cache_model_t iTLB;
static cache_model_t dTLB;
static cache_model_t TLB;

static void cache_close(cache_model_t* cache) {
	free(cache->entries);
	cache->curr_time = 0;
	cache->misses = 0;
}

static void cache_end(void) {
	cache_close(&iCache);
	cache_close(&dCache);
	cache_close(&iTLB);
	cache_close(&dTLB);
	cache_close(&TLB);
}


static void cache_init(
        int cache_id, 
        size_t sets, 
        size_t block_size, // argument passed in number of bytes
        size_t associativity) {

    cache_model_t* cache = NULL;

    switch(cache_id) {
        case ID_ICache: cache = &iCache; break;
        case ID_DCache: cache = &dCache; break;
        case ID_ITLB:   cache = &iTLB; break;
        case ID_DTLB:   cache = &dTLB; break;
        case ID_TLB:    cache = &TLB; break;
        default: exit(1); // Error non valid cache_id
    }

    // Get msb
    size_t total_bits = block_size*8;
    size_t bit_index = 0;
    while (total_bits >>= 1) {
            bit_index++;
            
    }

    cache->block_size = bit_index;
    cache->sets = sets;
    cache->associativity = associativity;
    cache->nb_entries = sets*associativity;
    cache->curr_time = 0;
    cache->misses = 0;
	cache->entries = calloc(cache->nb_entries, sizeof(cache_entry_t));
	memset(cache->entries, 0, cache->nb_entries*sizeof(cache_entry_t));
	if(!cache->entries) {
		exit(1);
	}
}

static inline size_t get_cache_tag(cache_model_t* cache, size_t addr) {
    return addr >> cache->block_size;
}

static inline size_t get_cache_slot(cache_model_t* cache, size_t addr) {
    size_t bits = addr >> cache->block_size;
    size_t slot = bits % cache->sets;
    return slot * cache->associativity;

}

static inline size_t cache_get_lru(cache_model_t* cache, size_t slot) {
	cache_entry_t entry = cache->entries[slot];
    size_t lru = slot;
    size_t time = entry.time;
    for(size_t i = 0; i < cache->associativity; i++) {
		entry = cache->entries[slot+i];
        if(entry.time < time) {
            lru = slot+i;
            time = entry.time;
        }

    }
    return lru;
}

static inline bool cache_access(cache_model_t* cache, size_t addr, char pid) {

    bool is_hit = false;
    size_t tag = get_cache_tag(cache, addr);
    size_t slot = get_cache_slot(cache, addr);
    size_t idx = 0;
	cache_entry_t* entry = &cache->entries[0];

    for(size_t i = 0; i < cache->associativity; i++) {
        idx = slot + i;
		entry = &cache->entries[idx];
        if(entry->valid && entry->pid == pid && entry->tag == tag) {
            is_hit = true;
            break;
        }
    }

    if(!is_hit) {
        idx = cache_get_lru(cache, slot);
		entry = &cache->entries[idx];
		entry->valid = true;
		entry->pid = pid;
        entry->tag = tag;
        cache->misses++;
    }
    entry->time = cache->curr_time;
    cache->curr_time++;

    return is_hit;
}

static void model_memaccess(mem_access_t mem) {
	bool is_hit = false;
	mem.isData ? total_mem++ : total_insts++;
	cache_model_t* tlb = mem.isData ? &dTLB : &iTLB;
	cache_model_t* cache = mem.isData ? &dCache : &iCache;

	is_hit = cache_access(tlb, mem.vaddr, mem.pid);
	if(!is_hit) {
		is_hit = cache_access(&TLB, mem.vaddr, mem.pid);
	}
	is_hit = cache_access(cache, mem.paddr, 0);

}

static inline void cache_log_stats(char *buffer, size_t max_size) { 

	// size_t tot_chars = snprintf(buffer, max_size,
	// 	"Memory System statistics:\n"
	// 	"  TOT Fetch Insts: %zu\n"
	// 	"    MISS $I  : %zu\n"
	// 	"    MISS ITLB: %zu\n"
	// 	"  TOT LD/ST Insnts: %zu\n"
	// 	"    MISS $D  : %zu\n"
	// 	"    MISS DTLB: %zu\n"
	// 	"\n"
	// 	"    MISS TLB : %zu\n", 
	// 	total_insts, iCache.misses, iTLB.misses, 
	// 	total_mem, dCache.misses, dTLB.misses,
	// 	TLB.misses);

	double iCachePercent = (double) iCache.misses / (double) total_insts;
	double dCachePercent = (double) dCache.misses / (double) total_mem;
	
	double iTLBPercent = (double) iTLB.misses / (double) total_insts;
	double dTLBPercent = (double) dTLB.misses / (double) total_mem;

	double TLBPercent = (double) TLB.misses / (double) (iTLB.misses + dTLB.misses);
	double PageFaultPercent = (double) TLB.misses / (double) (total_insts + total_mem);

	size_t tot_chars = snprintf(buffer, max_size, 
						 "%zu, %zu, %.5f, %zu, %zu, %.5f\n"
						 "%zu, %.5f, %zu, %.5f, %zu, %.5f, %.5f\n"
						 , 
						 total_insts, iCache.misses, iCachePercent, 
						 total_mem, dCache.misses, dCachePercent,
						 iTLB.misses, iTLBPercent, dTLB.misses, dTLBPercent, 
						 TLB.misses, TLBPercent, PageFaultPercent);


	if(tot_chars > max_size) {
		printf("Error");
		exit(1);
	}
}

static void cache_model_log_direct(void) {
   char str[256];
   cache_log_stats(str, 256);
   printf("%s", str);
}

static inline void parse_line_fast(char* line, mem_access_t* mem) {
	int isData = 0;
	int isStore = 0;
	sscanf(line, "CPU%i:%i:%i:0x%016lx:0x%016lx\n",
		   &mem->pid, &isData, &isStore, &mem->vaddr, &mem->paddr);
	mem->isData = isData ? true : false;
	mem->isStore = isStore ? true : false;
}

int main(int argc, char *argv[]) { 
	const char *filename = argv[1];
	FILE *file = fopen(filename, "r");
	char *line;
	size_t line_size = 0;
	mem_access_t mem;

	size_t sets, associativity, block_size;
	for(int cache_id = ID_ICache; cache_id < ID_UNDEF; cache_id++) {
		sets = atoi(argv[3*cache_id + 2]);
		associativity = atoi(argv[3*cache_id + 3]);
		block_size = atoi(argv[3*cache_id + 4]);
		cache_init(cache_id, sets, block_size, associativity);
		printf("%lu, %lu, %lu,\n", 
			   sets, associativity, block_size);
		//printf("%s: %lu %lu %lu\n", 
		//	   get_str[cache_id].str, sets, associativity, block_size);
	}

	while(getline(&line, &line_size, file) > 0) {
		parse_line_fast(line, &mem);
		model_memaccess(mem);
	}
	cache_model_log_direct();

	free(line);
	cache_end();

	return 0;
}
