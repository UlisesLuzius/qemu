#if defined(QFLEX)

/** 
 * HELPER definitions for TCG code generation.
 */
#if defined(TCG_GEN)
DEF_HELPER_2(qflex_magic_inst, void, env, i64)
DEF_HELPER_4(qflex_pre_mem, void, env, i64, i32, i32)
DEF_HELPER_4(qflex_post_mem, void, env, i64, i32, i32)
DEF_HELPER_1(qflex_exception_return, void, env)
DEF_HELPER_3(qflex_executed_instruction, void, env, i64, int)

#else 
// Empty definitions when disabled
void HELPER(qflex_magic_inst)(CPUARMState *env, uint64_t nop_op);
void HELPER(qflex_pre_mem)(CPUARMState *env, uint64_t addr, uint64_t type, uint32_t size);
void HELPER(qflex_post_mem)(CPUARMState *env, uint64_t addr, uint32_t type, uint32_t size);
void HELPER(qflex_exception_return)(CPUARMState *env);
void HELPER(qflex_executed_instruction)(CPUARMState* env, uint64_t pc, int location);

#endif

#if defined(TCG_GEN) && defined(CONFIG_DEVTEROFLEX)
DEF_HELPER_3(devteroflex_example_instrumentation, void, env, i64, i64)

#elif !defined(CONFIG_DEVTEROFLEX)
void HELPER(devteroflex_example_instrumentation)(CPUARMState *env, uint64_t arg1, uint64_t arg2);

#endif

#else /* !defined(CONFIG_QFLEX) */

// Make empty macros when deactivated
#if defined(TCG_GEN) 
#define helper_qflex_pre_mem(env, addr, type, size)
#define helper_qflex_post_mem(env, addr, type, size)
#else
void HELPER(qflex_pre_mem)(CPUARMState *env, uint64_t addr, uint64_t type, uint32_t size) { return; }
void HELPER(qflex_post_mem)(CPUARMState *env, uint64_t addr, uint32_t type, uint32_t size) { return; }
#endif

#endif /* defined(CONFIG_QFLEX) */