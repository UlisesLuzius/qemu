/** HELPER definitions for TCG code generation.
 */
#if defined(TCG_GEN) && defined(CONFIG_QFLEX)
DEF_HELPER_3(qflex_ldst_done, void, env, i64, i64)
DEF_HELPER_2(qflex_fetch_pc, void, env, i64)
DEF_HELPER_2(qflex_magic_insn, void, env, i64)
DEF_HELPER_1(qflex_exception_return, void, env)
DEF_HELPER_3(qflex_executed_instruction, void, env, i64, int)
DEF_HELPER_5(qflex_profile, void, env, i64, int, int, int)
#elif !defined(CONFIG_QFLEX) // Empty definitions when disabled
void HELPER(qflex_ldst_done)(CPUARMState *env, uint64_t addr, uint64_t isStore);
void HELPER(qflex_fetch_pc)(CPUARMState *env, uint64_t pc);
void HELPER(qflex_magic_insn)(CPUARMState *env, uint64_t nop_op);
void HELPER(qflex_exception_return)(CPUARMState *env);
void HELPER(qflex_executed_instruction)(CPUARMState* env, uint64_t pc, int location);
void HELPER(qflex_profile)(CPUARMState* env, uint64_t pc, int l1h, int l2h, int ldst);
#endif
