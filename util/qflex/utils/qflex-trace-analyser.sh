#! /bin/bash

# sets assiativity block_size
TLB=(1 1 1)
#TLB=(524288 16 4096) # Actually the page table

# alpha
ICache=(1024 4 64)
DCache=(1024 4 64)
#ITLB=(64 4 4096)
#DTLB=(64 4 4096)

# beta
#ICache=(8196 2 64)
#DCache=(8196 2 64)
ITLB=(1048 4 4096)
DTLB=(1048 4 4096)

for i in 16
do
echo "nb of cores $i"
./trace $i \
	${ICache[0]} ${ICache[1]} ${ICache[2]} \
	${DCache[0]} ${DCache[1]} ${DCache[2]} \
	${ITLB[0]}   ${ITLB[1]}   ${ITLB[2]}  \
	${DTLB[0]}   ${DTLB[1]}   ${DTLB[2]}  \
	${TLB[0]}    ${TLB[1]}    ${TLB[2]}
done

