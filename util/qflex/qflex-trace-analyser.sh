#! /bin/bash

CORE_COUNT=16
# sets assiativity block_size
ICache=(1024 4 64)
DCache=(1024 4 64)
ITLB=(64 4 4096)
DTLB=(64 4 4096)
TLB=(1 1 1) # Actually the page table

./trace 16 \
	${ICache[0]} ${ICache[1]} ${ICache[2]} \
	${DCache[0]} ${DCache[1]} ${DCache[2]} \
	${ITLB[0]}   ${ITLB[1]}   ${ITLB[2]}  \
	${DTLB[0]}   ${DTLB[1]}   ${DTLB[2]}  \
	${TLB[0]}    ${TLB[1]}    ${TLB[2]}
