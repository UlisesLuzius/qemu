#ifndef ARMLEX_COMMUNICATION_H
#define ARMLEX_COMMUNICATION_H

#include "qflex/armflex.h"
#include "qflex/armflex.pb-c.h"
#include "qflex/armflex-verification.h"

#define ARMFLEX_ROOT_DIR        "/dev/shm/qflex"

typedef struct ArmflexFile {
    void* region; // Mapped Memory Region
    int fd;       // File Discriptor
    size_t size;  // Size of region
} ArmflexFile;

int armflex_file_stream_open(FILE **fp, const char *filename);
int armflex_file_stream_write(FILE *fp, void *stream, size_t size);
	
/* Open a communication struct passing file 
 * Not a variable size stream, can only push a single structure of pre-defined size
 * Returns ArmflexFile
 */
int armflex_file_region_open(const char *filename, size_t size, ArmflexFile *file);

/* Closes an opened file*/
void armflex_file_region_close(ArmflexFile *file);

/* Write into already opened file region 
 * Buffer size must match region size
 */
void armflex_file_region_write(ArmflexFile *file, void* buffer);

/* Open shared memory location for specfic struct */
void* armflex_open_cmd_shm(const char* name, size_t struct_size);

/* ------ Protobuf ------ */
void *armflex_pack_protobuf(ArmflexArchState *armflex, size_t *size);
void armflex_unpack_protobuf(ArmflexArchState *armflex, void *stream, size_t size);
void armflex_pack_protobuf_trace(ArmflexCommitTrace *trace, ArmflexCommitTraceP *traceP, uint8_t *stream);
void armflex_trace_protobuf_close(ArmflexCommitTraceP *traceP, uint8_t **stream);
void armflex_trace_protobuf_open(ArmflexCommitTraceP *traceP, uint8_t **stream, size_t *size);

/* ---- Communication ---- */
/* This contains utilities to communicate between
 * the Chisel3 simulator and QEMU
 */
typedef struct ArmflexSimConfig_t {
    /* Names of files where information is shared between sim and QEMU
     */
    const char* sim_state;
    const char* sim_lock;
    const char* sim_cmd;

    const char* qemu_state;
    const char* qemu_lock;
    const char* qemu_cmd;

    const char* program_page;
    size_t page_size;

    /* Path where files are written and read from,
     * it should be something like /dev/shm/qflex
     */
    const char* rootPath;

    /* Path where to launcn simulator from (chisel3-project root),
     */
    const char* simPath;
} ArmflexSimConfig_t;

extern ArmflexSimConfig_t sim_cfg;


#endif
