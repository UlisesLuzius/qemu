#ifndef QFLEX_MODELS_H
#define QFLEX_MODELS_H

// ------ TRACE Memory Requests --------
void qflex_mem_trace_init(int core_count);
void qflex_inst_trace(uint32_t inst, uint64_t pid);
void qflex_mem_trace_start(size_t nb_insn);
void qflex_mem_trace_stop(void);
void qflex_mem_trace_end(void);
void qflex_mem_trace_memaccess(uint64_t addr, uint64_t hwaddr, 
							   uint64_t pid, uint64_t type);

void qflex_mem_trace_gen_helper_start(void);
void qflex_mem_trace_gen_helper_stop(void);
bool qflex_mem_trace_gen_helper(void);
bool qflex_mem_trace_gen_trace(void);


void qflex_mem_trace_log_stats(char* buffer, size_t max_size);
void qflex_mem_trace_log_direct(void);

#endif /* QFLEX_MODELS_H */
