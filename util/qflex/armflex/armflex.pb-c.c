/* Generated by the protocol buffer compiler.  DO NOT EDIT! */
/* Generated from: armflex.proto */

/* Do not generate deprecated warnings for self */
#ifndef PROTOBUF_C__NO_DEPRECATED
#define PROTOBUF_C__NO_DEPRECATED
#endif

#include "qflex/armflex/armflex.pb-c.h"
void   armflex_arch_state_p__init
                     (ArmflexArchStateP         *message)
{
  static const ArmflexArchStateP init_value = ARMFLEX_ARCH_STATE_P__INIT;
  *message = init_value;
}
size_t armflex_arch_state_p__get_packed_size
                     (const ArmflexArchStateP *message)
{
  assert(message->base.descriptor == &armflex_arch_state_p__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t armflex_arch_state_p__pack
                     (const ArmflexArchStateP *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &armflex_arch_state_p__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t armflex_arch_state_p__pack_to_buffer
                     (const ArmflexArchStateP *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &armflex_arch_state_p__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
ArmflexArchStateP *
       armflex_arch_state_p__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (ArmflexArchStateP *)
     protobuf_c_message_unpack (&armflex_arch_state_p__descriptor,
                                allocator, len, data);
}
void   armflex_arch_state_p__free_unpacked
                     (ArmflexArchStateP *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &armflex_arch_state_p__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
void   armflex_commit_trace_p__init
                     (ArmflexCommitTraceP         *message)
{
  static const ArmflexCommitTraceP init_value = ARMFLEX_COMMIT_TRACE_P__INIT;
  *message = init_value;
}
size_t armflex_commit_trace_p__get_packed_size
                     (const ArmflexCommitTraceP *message)
{
  assert(message->base.descriptor == &armflex_commit_trace_p__descriptor);
  return protobuf_c_message_get_packed_size ((const ProtobufCMessage*)(message));
}
size_t armflex_commit_trace_p__pack
                     (const ArmflexCommitTraceP *message,
                      uint8_t       *out)
{
  assert(message->base.descriptor == &armflex_commit_trace_p__descriptor);
  return protobuf_c_message_pack ((const ProtobufCMessage*)message, out);
}
size_t armflex_commit_trace_p__pack_to_buffer
                     (const ArmflexCommitTraceP *message,
                      ProtobufCBuffer *buffer)
{
  assert(message->base.descriptor == &armflex_commit_trace_p__descriptor);
  return protobuf_c_message_pack_to_buffer ((const ProtobufCMessage*)message, buffer);
}
ArmflexCommitTraceP *
       armflex_commit_trace_p__unpack
                     (ProtobufCAllocator  *allocator,
                      size_t               len,
                      const uint8_t       *data)
{
  return (ArmflexCommitTraceP *)
     protobuf_c_message_unpack (&armflex_commit_trace_p__descriptor,
                                allocator, len, data);
}
void   armflex_commit_trace_p__free_unpacked
                     (ArmflexCommitTraceP *message,
                      ProtobufCAllocator *allocator)
{
  if(!message)
    return;
  assert(message->base.descriptor == &armflex_commit_trace_p__descriptor);
  protobuf_c_message_free_unpacked ((ProtobufCMessage*)message, allocator);
}
static const ProtobufCFieldDescriptor armflex_arch_state_p__field_descriptors[4] =
{
  {
    "xregs",
    1,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(ArmflexArchStateP, n_xregs),
    offsetof(ArmflexArchStateP, xregs),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "pc",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT64,
    0,   /* quantifier_offset */
    offsetof(ArmflexArchStateP, pc),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "sp",
    3,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT64,
    0,   /* quantifier_offset */
    offsetof(ArmflexArchStateP, sp),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "nzcv",
    4,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(ArmflexArchStateP, nzcv),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned armflex_arch_state_p__field_indices_by_name[] = {
  3,   /* field[3] = nzcv */
  1,   /* field[1] = pc */
  2,   /* field[2] = sp */
  0,   /* field[0] = xregs */
};
static const ProtobufCIntRange armflex_arch_state_p__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 4 }
};
const ProtobufCMessageDescriptor armflex_arch_state_p__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "ArmflexArchState_p",
  "ArmflexArchStateP",
  "ArmflexArchStateP",
  "",
  sizeof(ArmflexArchStateP),
  4,
  armflex_arch_state_p__field_descriptors,
  armflex_arch_state_p__field_indices_by_name,
  1,  armflex_arch_state_p__number_ranges,
  (ProtobufCMessageInit) armflex_arch_state_p__init,
  NULL,NULL,NULL    /* reserved[123] */
};
static const ProtobufCFieldDescriptor armflex_commit_trace_p__field_descriptors[6] =
{
  {
    "state",
    1,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_MESSAGE,
    0,   /* quantifier_offset */
    offsetof(ArmflexCommitTraceP, state),
    &armflex_arch_state_p__descriptor,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "inst",
    2,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_UINT32,
    0,   /* quantifier_offset */
    offsetof(ArmflexCommitTraceP, inst),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "mem_addr",
    3,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(ArmflexCommitTraceP, n_mem_addr),
    offsetof(ArmflexCommitTraceP, mem_addr),
    NULL,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_PACKED,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "mem_data",
    4,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_UINT64,
    offsetof(ArmflexCommitTraceP, n_mem_data),
    offsetof(ArmflexCommitTraceP, mem_data),
    NULL,
    NULL,
    0 | PROTOBUF_C_FIELD_FLAG_PACKED,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "inst_block_data",
    5,
    PROTOBUF_C_LABEL_NONE,
    PROTOBUF_C_TYPE_BYTES,
    0,   /* quantifier_offset */
    offsetof(ArmflexCommitTraceP, inst_block_data),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
  {
    "mem_block_data",
    6,
    PROTOBUF_C_LABEL_REPEATED,
    PROTOBUF_C_TYPE_BYTES,
    offsetof(ArmflexCommitTraceP, n_mem_block_data),
    offsetof(ArmflexCommitTraceP, mem_block_data),
    NULL,
    NULL,
    0,             /* flags */
    0,NULL,NULL    /* reserved1,reserved2, etc */
  },
};
static const unsigned armflex_commit_trace_p__field_indices_by_name[] = {
  1,   /* field[1] = inst */
  4,   /* field[4] = inst_block_data */
  2,   /* field[2] = mem_addr */
  5,   /* field[5] = mem_block_data */
  3,   /* field[3] = mem_data */
  0,   /* field[0] = state */
};
static const ProtobufCIntRange armflex_commit_trace_p__number_ranges[1 + 1] =
{
  { 1, 0 },
  { 0, 6 }
};
const ProtobufCMessageDescriptor armflex_commit_trace_p__descriptor =
{
  PROTOBUF_C__MESSAGE_DESCRIPTOR_MAGIC,
  "ArmflexCommitTrace_p",
  "ArmflexCommitTraceP",
  "ArmflexCommitTraceP",
  "",
  sizeof(ArmflexCommitTraceP),
  6,
  armflex_commit_trace_p__field_descriptors,
  armflex_commit_trace_p__field_indices_by_name,
  1,  armflex_commit_trace_p__number_ranges,
  (ProtobufCMessageInit) armflex_commit_trace_p__init,
  NULL,NULL,NULL    /* reserved[123] */
};
