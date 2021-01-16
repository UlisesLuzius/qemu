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
	ArmflexArchStateP state = 
		ARMFLEX_ARCH_STATE_P__INIT;
	void *stream;

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
	stream = malloc (len);                                // Allocate required serialized buffer length 
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

