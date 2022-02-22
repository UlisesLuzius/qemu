#include <stdbool.h>
#include <stdio.h>
#include "qflex/qflex.h"
#include "qflex/devteroflex/devteroflex.h"
#include "qflex/devteroflex/devteroflex-mmu.h"
#include "qflex/devteroflex/demand-paging.h"

void devteroflex_mmu_flush_by_va_asid(uint64_t va, uint64_t asid) {
  for(int i = 0; i < 3; ++i){
    uint64_t to_evicted = IPT_COMPRESS(va, asid, i);
    if(tpt_lookup(to_evicted)) {
        // do simple page eviction.
      page_eviction_request(to_evicted);
      page_eviction_wait_complete(&to_evicted, 1);

      break;
    }
  }
}

void devteroflex_mmu_flush_by_asid(uint64_t asid) {
  // 1. get all entries in the tpt.
  uint64_t ele = 0;
  uint64_t *keys = tpt_all_keys(&ele);

  if(ele == 0){
    return;
  }

  uint64_t number_of_match = 0;
  // 2. query the number of matches.
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(keys[i]) == asid) {
      number_of_match++;
    }
  }

  if(number_of_match == 0){
    free(keys);
    return;
  }

  // 3. allocate memory to keep them.
  uint64_t *matched = calloc(number_of_match, sizeof(uint64_t));
  uint64_t matched_key_index = 0;
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(keys[i]) == asid) {
      matched[matched_key_index++] = keys[i];
    }
  }

  free(keys);

  // 4. do flushing.

  for(int i = 0; i < number_of_match; i++) {
      page_eviction_request(matched[i]);
  }
  page_eviction_wait_complete(matched, number_of_match);

  free(matched);
}


void devteroflex_mmu_flush_by_hva_asid(uint64_t hva, uint64_t asid) {
  // 1. query the IPT.
  uint64_t *ipt_synonyms;
  uint64_t ele = ipt_check_synonyms(hva, &ipt_synonyms);

  if(ele == 0){
    return;
  }

  uint64_t number_of_match = 0;
  // 2. filter
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(ipt_synonyms[i]) == asid) {
      number_of_match++;
    }
  }

  if(number_of_match == 0){
    free(ipt_synonyms);
    return;
  }

  // 3. allocate memory to keep them.
  uint64_t *matched = calloc(number_of_match, sizeof(uint64_t));
  uint64_t matched_key_index = 0;
  for(int i = 0; i < ele; ++i){
    if(IPT_GET_ASID(ipt_synonyms[i]) == asid) {
      matched[matched_key_index++] = ipt_synonyms[i];
    }
  }

  free(ipt_synonyms);

  // 4. do flushing.
  for(int i = 0; i < number_of_match; i++) {
      page_eviction_request(matched[i]);
  }
  page_eviction_wait_complete(matched, number_of_match);

  free(matched);
}


void devteroflex_mmu_flush_by_hva(uint64_t hva) {
  // 1. query the IPT.
  uint64_t *ipt_synonyms;
  uint64_t ele = ipt_check_synonyms(hva, &ipt_synonyms);

  if(ele == 0){
    return;
  }

  // 4. do flushing.
  for(int i = 0; i < ele; i++) {
      page_eviction_request(ipt_synonyms[i]);
  }
  page_eviction_wait_complete(ipt_synonyms, ele);

  free(ipt_synonyms);
}


void devteroflex_mmu_flush_all(void) {
  // 1. get all entries in the tpt.
  uint64_t ele = 0;
  uint64_t *keys = tpt_all_keys(&ele);

  if(ele == 0){
    return;
  }
  // 4. do flushing.

  for(int i = 0; i < ele; i++) {
      page_eviction_request(keys[i]);
  }
  page_eviction_wait_complete(keys, ele);

  free(keys);
}
