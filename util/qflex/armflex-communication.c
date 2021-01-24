#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <semaphore.h>
#include <string.h>

#include "qemu/osdep.h"
#include "qemu/thread.h"

#include "qflex/qflex-log.h"

#include "qflex/armflex.h"
#include "qflex/armflex-communication.h"
#include "qflex/armflex.pb-c.h"

int armflex_file_stream_open(FILE **fp, const char *filename) {
    char filepath[PATH_MAX];

    qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                   "Writing file : "ARMFLEX_ROOT_DIR"/%s\n", filename);
    if (mkdir(ARMFLEX_ROOT_DIR, 0777) && errno != EEXIST) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "'mkdir "ARMFLEX_ROOT_DIR"' failed\n");
        return 1;
    }

    snprintf(filepath, PATH_MAX, ARMFLEX_ROOT_DIR"/%s", filename);
	*fp = fopen(filepath, "w");
    if(!fp) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "ERROR: File stream open failed\n"
                       "    filepath:%s\n", filepath);
        return 1;
    }

    return 0;
}

int armflex_file_stream_write(FILE *fp, void *stream, size_t size) {
	if(fwrite(stream, 1, size, fp) != size) {
		fclose(fp);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
            "Error writing stream to file\n");
        return 1;
	}
	return 0;
}

int armflex_file_region_open(const char *filename, size_t size, ArmflexFile *file) {
    char filepath[PATH_MAX];
    int fd = -1;
    void *region;
    qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                   "Writing file : "ARMFLEX_ROOT_DIR"/%s\n", filename);
    if (mkdir(ARMFLEX_ROOT_DIR, 0777) && errno != EEXIST) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "'mkdir "ARMFLEX_ROOT_DIR"' failed\n");
        return 1;
    }
    snprintf(filepath, PATH_MAX, ARMFLEX_ROOT_DIR"/%s", filename);
    if((fd = open(filepath, O_RDWR | O_CREAT | O_TRUNC, 0666)) == -1) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
                       "Program Page dest file: open failed\n"
                       "    filepath:%s\n", filepath);
        return 1;
    }
    if (lseek(fd, size-1, SEEK_SET) == -1) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
            "Error calling lseek() to 'stretch' the file\n");
        return 1;
    }
    if (write(fd, "", 1) != 1) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
            "Error writing last byte of the file\n");
        return 1;
    }

    region = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if(region == MAP_FAILED) {
        close(fd);
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,
            "Error dest file: mmap failed");
        return 1;
    }

    file->fd=fd;
    file->region = region;
    file->size = size;
    return 0;
}

void armflex_file_region_write(ArmflexFile *file, void* buffer) {
    memcpy(file->region, buffer, file->size);
    msync(file->region, file->size, MS_SYNC);
}

void armflex_file_region_close(ArmflexFile *file) {
    munmap(file->region, file->size);
    close(file->fd);
}

void* armflex_open_cmd_shm(const char* name, size_t struct_size){
	int shm_fd; 
	shm_fd = shm_open(name, O_CREAT | O_RDWR, 0666); 
    if (ftruncate(shm_fd, struct_size) < 0) {
        qflex_log_mask(QFLEX_LOG_FILE_ACCESS,"ftruncate for '%s' failed\n",name);
    }
	void* cmd =  mmap(0, struct_size, PROT_READ|PROT_WRITE, MAP_SHARED, shm_fd, 0); 
	return cmd;
}

/* ------ Protobuf ------ */
void *armflex_pack_protobuf(ArmflexArchState *armflex, size_t *size) {
	void *stream;
	ArmflexArchStateP state = 
		ARMFLEX_ARCH_STATE_P__INIT;

	// Init fields
	state.n_xregs = 32;
	state.xregs = malloc(sizeof(uint64_t) * 32); // Allocate memory to store xregs
	assert(state.xregs);

	// armflex struct -> protobuf struct
	memcpy(state.xregs, armflex->xregs, sizeof(uint64_t) * 32);
	state.pc = armflex->pc;
	state.sp = armflex->sp;
	state.nzcv = armflex->nzcv;

	// protobuf struct -> protobuf stream
	size_t len = armflex_arch_state_p__get_packed_size (&state); // This is calculated packing length
	stream = malloc(len);
	armflex_arch_state_p__pack (&state, stream);          // Pack the data

	// Free allocated fields
	free (state.xregs);

	// Return stream
	*size = len;
	return stream;
}

void armflex_unpack_protobuf(ArmflexArchState *armflex, void *stream, size_t size) {
	if(armflex == NULL) {
		fprintf(stderr, "Alloc ARMFlex state ptr before passing it as argument\n");
		exit(1);
	}

	// protobuf stream -> protobuf struct
	ArmflexArchStateP *state;
	state = armflex_arch_state_p__unpack(NULL, size, stream); // Deserialize the serialized input
	if (state == NULL) {
		fprintf(stderr, "Error unpacking ARMFLEX state protobuf message\n");
		exit(1);
	}

	// protobuf struct -> armflex struct
	memcpy(&armflex->xregs, &state->xregs, sizeof(uint64_t) * 32);
	armflex->pc = state->pc;
	armflex->sp = state->sp;
	armflex->nzcv = state->nzcv;

	// Free protobuf struct
	armflex_arch_state_p__free_unpacked(state, NULL); // Free the message from unpack()
}


void armflex_trace_protobuf_open(ArmflexCommitTraceP *traceP,
										uint8_t **stream, size_t *size) {
	// Init fields
	traceP->n_mem_addr = MAX_MEM_INSTS;
	traceP->mem_addr = malloc (sizeof (uint64_t) * MAX_MEM_INSTS);
	traceP->n_mem_data = MAX_MEM_INSTS;
	traceP->mem_data = malloc (sizeof (uint64_t) * MAX_MEM_INSTS);
	assert(traceP->mem_addr);
	assert(traceP->mem_data);

	// Init State
	ArmflexArchStateP *state = traceP->state;
	state->n_xregs = 32;
	state->xregs = malloc (sizeof (uint64_t) * 32);
	assert(state->xregs);

	*size = armflex_commit_trace_p__get_packed_size (traceP); // This is calculated packing length
	*stream = malloc(*size + 1);                               // Allocate required serialized buffer length 
	assert(stream);
}

void armflex_trace_protobuf_close(ArmflexCommitTraceP *traceP,
										 uint8_t **stream) {
	// Free allocated fields
	free(traceP->mem_addr);
	free(traceP->mem_data);
	free(traceP->state->xregs);
	free(*stream);
}

void armflex_pack_protobuf_trace(ArmflexCommitTrace *trace,
										ArmflexCommitTraceP *traceP, 
										uint8_t *stream) {
	// Pack Commit Trace
	traceP->inst = trace->inst;
	memcpy(traceP->mem_addr, trace->mem_addr, sizeof(uint64_t) * traceP->n_mem_addr);
	memcpy(traceP->mem_data, trace->mem_data, sizeof(uint64_t) * traceP->n_mem_data);

	// Pack Armflex State
	ArmflexArchState *state = &trace->state;
	ArmflexArchStateP *stateP = traceP->state;
	memcpy(stateP->xregs, state->xregs, sizeof(uint64_t) * stateP->n_xregs);
	stateP->pc = state->pc;
	stateP->sp = state->sp;
	stateP->nzcv = state->nzcv;

	// protobuf struct -> protobuf stream
	armflex_commit_trace_p__pack (traceP, stream); // Pack the data
}


