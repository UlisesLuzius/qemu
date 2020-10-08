// OP TYPES
#define QFLEX_OP      (90)
#define MEM_TRACE_OP      (91)

// MEM_TRACE OPs
#define MEM_TRACE_START   (90)
#define MEM_TRACE_STOP    (91)
#define MEM_TRACE_RESULTS (92)

// QFLEX OPs



// ---- Executing helpers inside QEMU ---- //
#define STR(x) #x
#define XSTR(s) STR(s)
#define magic_inst(val) __asm__ __volatile__ ( "hint " XSTR(val) " \n\t"  )

#define DO_MEM_TRACE_OP(op) magic_inst(MEM_TRACE_OP); magic_inst(op)
