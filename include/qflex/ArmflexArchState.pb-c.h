/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: ArmflexArchState.proto */

#ifndef PROTOBUF_C_ArmflexArchState_2eproto__INCLUDED
#define PROTOBUF_C_ArmflexArchState_2eproto__INCLUDED

#include <protobuf-c/protobuf-c.h>

PROTOBUF_C__BEGIN_DECLS

#if PROTOBUF_C_VERSION_NUMBER < 1003000
# error This file was generated by a newer version of protoc-c which is incompatible with your libprotobuf-c headers. Please update your headers.
#elif 1003003 < PROTOBUF_C_MIN_COMPILER_VERSION
# error This file was generated by an older version of protoc-c which is incompatible with your libprotobuf-c headers. Please regenerate this file with a newer version of protoc-c.
#endif


typedef struct _ArmflexArchStateP ArmflexArchStateP;


/* --- enums --- */


/* --- messages --- */

struct  _ArmflexArchStateP
{
  ProtobufCMessage base;
  size_t n_xregs;
  uint64_t *xregs;
  uint64_t pc;
  uint64_t sp;
  uint32_t nzcv;
};
#define ARMFLEX_ARCH_STATE_P__INIT \
 { PROTOBUF_C_MESSAGE_INIT (&armflex_arch_state_p__descriptor) \
    , 0,NULL, 0, 0, 0 }


/* ArmflexArchStateP methods */
void   armflex_arch_state_p__init
                     (ArmflexArchStateP         *message);
size_t armflex_arch_state_p__get_packed_size
                     (const ArmflexArchStateP   *message);
size_t armflex_arch_state_p__pack
                     (const ArmflexArchStateP   *message,
                      uint8_t             *out);
size_t armflex_arch_state_p__pack_to_buffer
                     (const ArmflexArchStateP   *message,
                      ProtobufCBuffer     *buffer);
ArmflexArchStateP *
       armflex_arch_state_p__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data);
void   armflex_arch_state_p__free_unpacked
                     (ArmflexArchStateP *message,
                      ProtobufCAllocator *allocator);
/* --- per-message closures --- */

typedef void (*ArmflexArchStateP_Closure)
                 (const ArmflexArchStateP *message,
                  void *closure_data);

/* --- services --- */


/* --- descriptors --- */

extern const ProtobufCMessageDescriptor armflex_arch_state_p__descriptor;

PROTOBUF_C__END_DECLS


#endif  /* PROTOBUF_C_ArmflexArchState_2eproto__INCLUDED */
