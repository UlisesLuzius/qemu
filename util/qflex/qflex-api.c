//  DO-NOT-REMOVE begin-copyright-block
// QFlex consists of several software components that are governed by various
// licensing terms, in addition to software that was developed internally.
// Anyone interested in using QFlex needs to fully understand and abide by the
// licenses governing all the software components.
// 
// ### Software developed externally (not by the QFlex group)
// 
//     * [NS-3] (https://www.gnu.org/copyleft/gpl.html)
//     * [QEMU] (http://wiki.qemu.org/License)
//     * [SimFlex] (http://parsa.epfl.ch/simflex/)
//     * [GNU PTH] (https://www.gnu.org/software/pth/)
// 
// ### Software developed internally (by the QFlex group)
// **QFlex License**
// 
// QFlex
// Copyright (c) 2020, Parallel Systems Architecture Lab, EPFL
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
// 
//     * Redistributions of source code must retain the above copyright notice,
//       this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright notice,
//       this list of conditions and the following disclaimer in the documentation
//       and/or other materials provided with the distribution.
//     * Neither the name of the Parallel Systems Architecture Laboratory, EPFL,
//       nor the names of its contributors may be used to endorse or promote
//       products derived from this software without specific prior written
//       permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE PARALLEL SYSTEMS ARCHITECTURE LABORATORY,
// EPFL BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
// GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
// HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
// LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//  DO-NOT-REMOVE end-copyright-block
#ifdef __cplusplus
extern "C" {
#endif

#include "qemu/osdep.h"            // Added for flexus because in the new version of QEMU qmp-commands.h uses type error that it doesnt call itself . osdep calls it for qmp-commands.h
#include "cpu.h"
#include "qom/cpu.h"
#include "qemu/config-file.h"
#include "sysemu/cpus.h"
#include "qmp-commands.h"
#include "include/exec/exec-all.h"
#include "api.h"

#include "target/arm/cpu.h"

static int pending_exception = 0;
static int64_t simulationTime = -1;
static bool timing;
static int debugStats[ALL_DEBUG_TYPE] = {0};
static QEMU_callback_table_t * QEMU_all_callbacks_tables = NULL;
static conf_object_t *qemu_cpus = NULL;
static conf_object_t *qemu_mems = NULL;
static conf_object_t* qemu_disas_context = NULL;
static bool qemu_objects_initialized;

#ifdef CONFIG_QUANTUM
extern uint64_t quantum_value;
#endif
extern int smp_cpus;
extern int smp_sockets;

static int flexus_is_simulating;

uint64_t QEMU_read_unhashed_sysreg(conf_object_t *c, uint8_t op0, uint8_t op1, uint8_t op2, uint8_t crn, uint8_t crm) {
    /* First get ARMCPU and the list of sysregs */
    CPUState *cs = (CPUState*)c->object;
    ARMCPU *cpu = ARM_CPU(cs);
    CPUARMState *env = &cpu->env;

    const ARMCPRegInfo *ri;
    ri = get_arm_cp_reginfo(cpu->cp_regs,
                            ENCODE_AA64_CP_REG(CP_REG_ARM64_SYSREG_CP,
                                               crn, crm, op0, op1, op2));

    if (ri && !(ri->type & ARM_CP_NO_RAW)) { 
        return read_raw_cp_reg(env,ri);
    } else { /* Msutherl: do it the slow way by linear searching if previous encoding didn't work */
        for (size_t i = 0; i < cpu->cpreg_array_len; i++) {
            uint32_t regidx = kvm_to_cpreg_id(cpu->cpreg_indexes[i]);
            ri = get_arm_cp_reginfo(cpu->cp_regs, regidx);
            if (ri->opc0 == op0 &&
                ri->opc1 == op1 &&
                ri->opc2 == op2 &&
                ri->crn == crn &&
                ri->crm == crm && 
                !(ri->type & ARM_CP_NO_RAW)) {
                return read_raw_cp_reg(env,ri);
            }
        }
    }
    /* Unknown register; this might be a guest error or a QEMU
     * unimplemented feature.
     */
    qemu_log_mask(LOG_UNIMP, "Libqflex: %s access to unsupported AArch64 "
                  "system register op0:%d op1:%d crn:%d crm:%d op2:%d\n",
                  "read", op0, op1, crn, crm, op2);
    return 0xcafebabedeadbeef;
}

void QEMU_dump_state(conf_object_t* cpu, char** buf) {
    qemu_dump_state(cpu->object, buf);
}

char* QEMU_disassemble(conf_object_t* cpu, uint64_t pc){
    return disassemble(cpu->object, pc);
}

void QEMU_write_phys_memory(conf_object_t *cpu, physical_address_t pa, unsigned long long value, int bytes){
    assert(0 <= bytes && bytes <= 16);
    /* allocate raw buffer to pass to qemu */
    uint8_t buf[sizeof(unsigned long long)];
    memcpy(buf, &value, bytes);
    cpu_physical_memory_write(pa, buf, bytes);
}

int QEMU_clear_exception(void){
    assert(false);
}

void QEMU_write_register(conf_object_t *cpu, arm_register_t reg_type, int reg_index, uint64_t value)
{
  assert(cpu->type == QEMU_CPUState);
  cpu_write_register( cpu->object, reg_type, reg_index, value );
  assert(cpu_read_register(cpu->object, reg_type, reg_index) == value);
}

uint64_t QEMU_read_register(conf_object_t *cpu, arm_register_t reg_type, int reg_index) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_register(cpu->object, reg_type, reg_index);
}

uint32_t QEMU_read_pstate(conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_pstate(cpu->object);
}

uint64_t QEMU_read_hcr_el2(conf_object_t* cpu){
    assert(cpu->type == QEMU_CPUState);
    return cpu_read_hcr_el2(cpu);
}

void QEMU_read_exception(conf_object_t *cpu, exception_t* exp) {
  assert(cpu->type == QEMU_CPUState);
  cpu_read_exception(cpu->object, exp);
}

uint64_t QEMU_get_pending_interrupt(conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_get_pending_interrupt(cpu->object);
}

uint32_t QEMU_read_DCZID_EL0(conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_DCZID_EL0(cpu->object);
}

bool QEMU_read_AARCH64(conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_AARCH64(cpu->object);
}

uint64_t QEMU_read_sp_el(uint8_t id, conf_object_t *cpu){
    assert(cpu->type == QEMU_CPUState);
    return cpu_read_sp_el(id, cpu->object);
}

bool QEMU_cpu_has_work(conf_object_t *cpu){
    assert(cpu->type == QEMU_CPUState);
    return ! cpu_is_idle(cpu->object);
}

uint64_t QEMU_read_sctlr(uint8_t id, conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_sctlr(id, cpu->object);
}

uint64_t QEMU_read_tpidr(uint8_t id, conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_tpidr(id, cpu->object);
}

uint32_t QEMU_read_fpsr(conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_fpsr(cpu->object);
}

uint32_t QEMU_read_fpcr(conf_object_t *cpu) {
  assert(cpu->type == QEMU_CPUState);
  return cpu_read_fpcr(cpu->object);
}

conf_object_t * QEMU_get_phys_memory(conf_object_t *cpu){

    if (qemu_objects_initialized)
        return &(qemu_mems[QEMU_get_cpu_index(cpu)]);
    assert(false);
}



void QEMU_read_phys_memory(uint8_t* buf, physical_address_t pa, int bytes)
{
  assert(0 <= bytes && bytes <= 16);
  cpu_physical_memory_read(pa, buf, bytes);
}

void init_qemu_disas_context(uint8_t cpu_idx, void* obj){
    if (qemu_disas_context == NULL)
        qemu_disas_context = malloc(sizeof(conf_object_t) * QEMU_get_num_cores());

    qemu_disas_context[cpu_idx].object = obj;
    qemu_disas_context[cpu_idx].type = QEMU_DisasContext;
    qemu_disas_context[cpu_idx].name = strdup("DisasContext");
}

void update_qemu_disas_context(uint8_t cpu_idx, void* obj){
    assert(qemu_disas_context != NULL);
    qemu_disas_context[cpu_idx].object = obj;
}


void* get_qemu_disas_context(uint8_t cpu_idx){
    if (qemu_disas_context != NULL)
    return qemu_disas_context[cpu_idx].object;

    return NULL;
}

conf_object_t *QEMU_get_cpu_by_index(int index)
{
    if (qemu_objects_initialized)
        return &qemu_cpus[index];
    assert(false);
}

int QEMU_get_cpu_index(conf_object_t *cpu){
    return cpu_proc_num(cpu->object);
}

conf_object_t *QEMU_get_phys_mem(conf_object_t *cpu) {
  return NULL;
}

uint64_t QEMU_step_count(conf_object_t *cpu){
  return QEMU_get_instruction_count(QEMU_get_cpu_index(cpu), BOTH_INSTR);
}



int QEMU_get_num_cpus(void) {
    return smp_cpus;
}

int QEMU_get_num_sockets(void) {
  unsigned nsockets = 1;
  QemuOpts * opts = (QemuOpts*)qemu_opts_find( qemu_find_opts("smp-opts"), NULL );
  if( opts != NULL )
    nsockets = qemu_opt_get_number(opts, "sockets", 0);
  return  nsockets > 0 ? nsockets : 1;
}

int QEMU_get_num_cores(void) {
  return smp_cores;
}

int QEMU_get_num_threads_per_core(void) {
  return smp_threads;
}

// return the id of the socket of the processor
//int QEMU_cpu_get_socket_id(conf_object_t *cpu) {
//  int threads_per_core = QEMU_get_num_threads_per_core();
//  int cores_per_socket = QEMU_get_num_cores();
//  int cpu_id = QEMU_get_processor_number(cpu);
//  return cpu_id / (cores_per_socket * threads_per_core);
//}

conf_object_t * QEMU_get_all_cpus(void) {
    if (qemu_objects_initialized)
        return qemu_cpus;

    assert(false);
}

int QEMU_set_tick_frequency(conf_object_t *cpu, double tick_freq) 
{
	return 42;
}

double QEMU_get_tick_frequency(conf_object_t *cpu){
	return 3.14;
}

uint64_t QEMU_get_program_counter(conf_object_t * cpu)
{
    assert(cpu->type == QEMU_CPUState);
    return cpu_get_program_counter(cpu->object);
}

void QEMU_increment_debug_stat(int val)
{
    debugStats[val]++;
}
physical_address_t QEMU_logical_to_physical(conf_object_t *cpu, 
		data_or_instr_t fetch, logical_address_t va) 
{
  assert(cpu->type == QEMU_CPUState);
  CPUState * qemucpu = cpu->object;

  return mmu_logical_to_physical(qemucpu, va);
}

//------------------------Should be fairly simple here-------------------------

//[???] I assume these are treated as booleans were 1 = true, 0 = false
int QEMU_mem_op_is_data(generic_transaction_t *mop)
{
	//[???]not sure on this one
	//possibly
	//if it is read or write. Might also need cache?
	if((mop->type == QEMU_Trans_Store) || (mop->type == QEMU_Trans_Load)){
		return 1;
	}else{
		return 0;
	}
}

int QEMU_mem_op_is_write(generic_transaction_t *mop)
{
	//[???]Assuming Store == write.
	if(mop->type == QEMU_Trans_Store){
		return 1;
	}else{
		return 0;
	}
}

int QEMU_mem_op_is_read(generic_transaction_t *mop)
{
	//[???]Assuming load == read.
	if(mop->type == QEMU_Trans_Load){
		return 1;
	}else{
		return 0;
	}
}

instruction_error_t QEMU_instruction_handle_interrupt(conf_object_t *cpu, pseudo_exceptions_t pendingInterrupt) {
	return QEMU_IE_OK;
}
 
int QEMU_get_pending_exception(void) {
    return pending_exception;
} 

conf_object_t * QEMU_get_object_by_name(const char *name) {

    if (!qemu_objects_initialized)
        assert(false);

    unsigned int i;
    for(i = 0; i < QEMU_get_num_cpus(); i++) {
        if(strcmp(qemu_cpus[i].name, name) == 0){
            return &(qemu_cpus[i]);
        }
        if(strcmp(qemu_mems[i].name, name) == 0){
            return &(qemu_mems[i]);
        }
	}
    return NULL;
}

int QEMU_cpu_execute (conf_object_t *cpu, bool count_time) {
  
   assert(cpu->type == QEMU_CPUState);

  int ret = 0;
  CPUState * cpu_state = cpu->object;
  pending_exception = cpu_state->exception_index;
  ret = advance_qemu(cpu->object);
  if (count_time) {
      simulationTime--;
  }
  return ret;
}


int QEMU_is_in_simulation(void) {
  return flexus_is_simulating;
}

void QEMU_toggle_simulation(int enable) {
  if( enable != QEMU_is_in_simulation() ) {
    flexus_is_simulating = enable;
    QEMU_flush_tb_cache();
  }
}

void QEMU_flush_tb_cache(void) {
  CPUState* cpu = first_cpu;
  CPU_FOREACH(cpu) {
    tb_flush(cpu);
  }
}


static void QEMU_populate_qemu_mems(void)
{
    int ncpus = QEMU_get_num_cpus();
    qemu_mems = malloc(sizeof(conf_object_t)*ncpus);

    int i = 0;
    for ( ; i < ncpus; i++) {
        qemu_mems[i].type = QEMU_AddressSpace;

        qemu_mems[i].name = (char *)malloc(sizeof(qemu_cpus[i].name) + sizeof("_mem " ));
        sprintf(qemu_mems[i].name,"%s_mem",qemu_cpus[i].name);
        qemu_mems[i].object = (AddressSpace *)qemu_cpu_get_address_space(qemu_cpus[i].object);
        if(! qemu_mems[i].object){
            assert(false);
        }

    }
}

static void QEMU_populate_qemu_cpus(void)
{
    int ncpus = QEMU_get_num_cpus();
    qemu_cpus = malloc(sizeof(conf_object_t)*ncpus);

    int i = 0;
    for ( ; i < ncpus; i++) {
        qemu_cpus[i].type = QEMU_CPUState;

        int needed = snprintf(NULL, 0, "cpu%d", i ) + 1;
        qemu_cpus[i].name = malloc(needed);
        snprintf(qemu_cpus[i].name, needed, "cpu%d", i);
        qemu_cpus[i].object = qemu_get_cpu(i);
        if(! qemu_cpus[i].object){
            assert(false);
        }

    }
}

static void QEMU_setup_qemu_objects(void){
    QEMU_populate_qemu_cpus();
    QEMU_populate_qemu_mems();
}

void QEMU_initialize(bool timing_mode) {

  if (qemu_objects_initialized)
      assert(false);

  timing = timing_mode;

  QEMU_initialize_counts();
  QEMU_setup_callback_tables();
  QEMU_setup_qemu_objects();

  qemu_objects_initialized = true;

}

void QEMU_shutdown(void) {
  QEMU_free_callback_tables();
  QEMU_deinitialize_counts();
}

uint64_t QEMU_total_instruction_count = 0;
uint64_t *QEMU_instruction_counts = NULL;
uint64_t *QEMU_instruction_counts_user = NULL;
uint64_t *QEMU_instruction_counts_OS = NULL;

void QEMU_initialize_counts(void) {
  int num_cpus = QEMU_get_num_cpus();
  QEMU_instruction_counts= (uint64_t*)malloc(num_cpus*sizeof(uint64_t));

  QEMU_instruction_counts_user = (uint64_t*)malloc(num_cpus*sizeof(uint64_t));
  QEMU_instruction_counts_OS = (uint64_t*)malloc(num_cpus*sizeof(uint64_t));

  QEMU_total_instruction_count = 0;
  int i = 0;
  for( ; i < num_cpus; i++ ){
      QEMU_instruction_counts[i] = 0;

    QEMU_instruction_counts_user[i] = 0;
    QEMU_instruction_counts_OS[i] = 0;
  }
}

void QEMU_deinitialize_counts(void) {
    free(QEMU_instruction_counts);
  free(QEMU_instruction_counts_user);
  free(QEMU_instruction_counts_OS);
}

int64_t QEMU_getSimulationTime(void)
{
    return simulationTime;
}
void QEMU_setSimulationTime(uint64_t time)
{
    simulationTime = time;
}

static int qemu_stopped = 0;

int QEMU_is_stopped(void)
{
    return qemu_stopped;
}


conf_object_t *QEMU_get_ethernet(void) {
    return NULL;
}

//[???]Not sure what this does if there is a simulation_break, shouldn't there be a simulation_resume?
bool QEMU_break_simulation(const char * msg)
{
    if (!QEMU_is_stopped()) {
        flexus_is_simulating = 0;
        qemu_stopped = 1;

        //[???]it could be pause_all_vcpus(void)
        //Causes the simulation to pause, can be restarted in qemu monitor by calling stop then cont
        //or can be restarted by calling resume_all_vcpus();
        //looking at it some functtions in vl.c might be useful
        printf("Exiting because of break_simulation\n");
        printf("With exit message: %s\n", msg);

        Error* error_msg = NULL;
        error_setg(&error_msg, "%s", msg);
        qmp_quit(&error_msg);
        error_free(error_msg);
    }

    //qemu_system_suspend();//from vl.c:1940//doesn't work at all
    //calls pause_all_vcpus(), and some other stuff.
    //For QEMU to know that they are paused I think.
    //qemu_system_suspend_request();//might be better from vl.c:1948
    //sort of works, but then resets cpus I think


    //I have not found anything that lets you send a message when you pause the simulation, but there can be a wakeup messsage.
    //in vl.c
//    int num_cpus = QEMU_get_num_cpus();
#ifdef CONFIG_DEBUG_LIBQFLEX
    printf ("----------API-OUTPUT----------\n");

    printf ("FETCH ops in io space:         %9i\n",debugStats[FETCH_IO_MEM_OP]);
    printf ("FETCH ops in RAM/ROM:          %9i\n",debugStats[FETCH_MEM_OP]);
    printf ("non fetch op in io space:      %9i\n",debugStats[NON_FETCH_IO_MEM_OP]);
    printf ("Ops not in io space:           %9i\n",debugStats[NON_IO_MEM_OP]);
    printf ("num cpu memory transactions:   %9i\n",debugStats[CPUMEMTRANS]);
    printf ("num ALL CALLBACKS:             %9i\n",debugStats[ALL_CALLBACKS]);
    printf ("num GENERIC CALLBACKS:         %9i\n",debugStats[ALL_GENERIC_CALLBACKS]);
    printf ("num NON_EXISTING_EVENT:        %9i\n",debugStats[NON_EXISTING_EVENT]);


    printf ("----------QEMU-OUTPUT----------\n");
    printf ("Transactions:       %9i\n",debugStats[NUM_DMA_ALL]);

    printf ("CALLBACKS:          %9i\n",debugStats[QEMU_CALLBACK_CNT]);
    printf ("                    %9s\t%9s\t%9s\n","USER","OS","ALL");

    printf ("Total Instructions: %9i\t%9i\t%9i\n",debugStats[USER_INSTR_CNT]
                                                   ,debugStats[OS_INSTR_CNT]
                                                   ,debugStats[ALL_INSTR_CNT]);

    printf ("Total Fetches:      %9i\t%9i\t%9i\n",debugStats[USER_FETCH_CNT]
                                                   ,debugStats[OS_FETCH_CNT]
                                                   ,debugStats[ALL_FETCH_CNT]);

    printf ("Loads:              %9i\t%9i\t%9i\n",debugStats[LD_USER_CNT]
                                                   ,debugStats[LD_OS_CNT]
                                                   ,debugStats[LD_ALL_CNT]);

    printf ("Stores:             %9i\t%9i\t%9i\n",debugStats[ST_USER_CNT]
                                                   ,debugStats[ST_OS_CNT]
                                                   ,debugStats[ST_ALL_CNT]);

    printf ("Cache Ops:          %9i\t%9i\t%9i\n",debugStats[CACHEOPS_USER_CNT]
                                                   ,debugStats[CACHEOPS_OS_CNT]
                                                   ,debugStats[CACHEOPS_ALL_CNT]);

    printf ("Transactions:       %9i\t%9i\t%9i\n",debugStats[NUM_TRANS_USER]
                                                 ,debugStats[NUM_TRANS_OS]
                                                   ,debugStats[NUM_TRANS_ALL]);

#endif

    return true;
}



uint64_t QEMU_get_instruction_count(int cpu_number, int isUser) {

    if (isUser == USER_INSTR ){
        return QEMU_instruction_counts_user[cpu_number];
    } else if (isUser == OS_INSTR ){
        return QEMU_instruction_counts_OS[cpu_number];
    } else {
        return QEMU_instruction_counts[cpu_number];
    }
}

void QEMU_increment_instruction_count(int cpu_number, int isUser) {

    if(isUser == USER_INSTR){
        QEMU_instruction_counts_user[cpu_number]++;
    } else {
        QEMU_instruction_counts_OS[cpu_number]++;
    }
    QEMU_instruction_counts[cpu_number]++;
    QEMU_total_instruction_count++;

}

uint64_t QEMU_get_total_instruction_count(void) {
  return QEMU_total_instruction_count;
}

void QEMU_setup_callback_tables(void) {
  int numTables = QEMU_get_num_cpus() + 1;
  QEMU_all_callbacks_tables = (QEMU_callback_table_t*)malloc(sizeof(QEMU_callback_table_t)*numTables);
  int i = 0;
  for( ; i < numTables; i++ ) {
    QEMU_callback_table_t * table = QEMU_all_callbacks_tables + i;
    table->next_callback_id = 0;
    int j = 0;
    for( ; j < QEMU_callback_event_count; j++ ) {
      table->callbacks[j] = NULL;
    }
  }
}

void QEMU_free_callback_tables(void) {
  int numTables = QEMU_get_num_cpus() + 1;
  int i = 0;
  for( ; i < numTables; i++ ) {
    QEMU_callback_table_t * table = QEMU_all_callbacks_tables + i;
    int j = 0;
    for( ; j < QEMU_callback_event_count; j++ ) {
      while( table->callbacks[j] != NULL ) {
        QEMU_callback_container_t *next = table->callbacks[j]->next;
        free(table->callbacks[j]);
        table->callbacks[j] = next;
      }
    }
  }
  free(QEMU_all_callbacks_tables);
}
// note: see QEMU_callback_table in api.h
// return a unique identifier to the callback struct or -1
// if an error occured
int QEMU_insert_callback( int cpu_id, QEMU_callback_event_t event, void* obj, void* fn) {
  //[???]use next_callback_id then update it
  //If there are multiple callback functions, we must chain them together.
  //error checking-
  if(event>=QEMU_callback_event_count){
    //call some sort of errorthing possibly
    return -1;
  }
  QEMU_callback_table_t * table = &QEMU_all_callbacks_tables[cpu_id+1];
  QEMU_callback_container_t *container = table->callbacks[event];
  QEMU_callback_container_t *containerNew = malloc(sizeof(QEMU_callback_container_t));

  // out of memory, return -1
  if( containerNew == NULL )
    return -1;

  containerNew->id = table->next_callback_id;
  containerNew->callback = fn;
  containerNew->obj = obj;
  containerNew->next = NULL;

  if(container == NULL){
    //Simple case there is not a callback function for event 
    table->callbacks[event] = containerNew;
  }else{
    //we need to add another callback to the chain
    //Now find the current last function in the callbacks list
    while(container->next!=NULL){
      container = container->next;
    }
    container->next = containerNew;
  }
  table->next_callback_id++;
  return containerNew->id;
}

// delete a callback specific to the given cpu
void QEMU_delete_callback(int cpu_id, QEMU_callback_event_t event, uint64_t callback_id) {
  //need to point prev->next to current->next
  //start with the first in the list and check its ID
  QEMU_callback_table_t * table = &QEMU_all_callbacks_tables[cpu_id+1];
  QEMU_callback_container_t *container = table->callbacks[event];
  QEMU_callback_container_t *prev = NULL;

  while( container != NULL ) {
    if( container->id == callback_id ) {
      // we have found a callback with the right id and cpu_id
      if( prev != NULL ) {
        // this is not the first element of he list
        // remove the element from the list
        prev->next = container->next;
      }else {
        // this is the first element of the list
        table->callbacks[event] = container->next;
      }
      free(container);
      break;
    }
    prev = container;
    container = container->next;
  }
}

static void do_execute_callback(QEMU_callback_container_t *curr, QEMU_callback_event_t event, QEMU_callback_args_t *event_data) {
    if (QEMU_is_stopped()) return;

  void *callback = curr->callback;
  switch (event) {
    // noc : class_data, conf_object_t
  case QEMU_config_ready:
    (*(cb_func_void)callback)(
							);
               
    break;
  case QEMU_magic_instruction:
    if (!curr->obj)
      (*(cb_func_nocI_t)callback)(
				  event_data->nocI->class_data
				  , event_data->nocI->obj
				  , event_data->nocI->bigint
				  );
    else
      (*(cb_func_nocI_t2)callback)(
				   (void*)curr->obj,
				   event_data->nocI->class_data
				   , event_data->nocI->obj
				   , event_data->nocI->bigint
				   );   
    break;
  case QEMU_continuation:
  case QEMU_asynchronous_trap:
  case QEMU_exception_return:
  case QEMU_ethernet_network_frame:
  case QEMU_ethernet_frame:
  case QEMU_periodic_event:
    if (!curr->obj){
      (*(cb_func_noc_t)callback)(
				 event_data->noc->class_data
				 , event_data->noc->obj
				 );
    }else{

      (*(cb_func_noc_t2)callback)(
				  (void *) curr->obj
				  , event_data->noc->class_data
				  , event_data->noc->obj
				  );
    }
    break;
    // nocIs : class_data, conf_object_t, int64_t, char*
  case QEMU_simulation_stopped:
    if (!curr->obj)
      (*(cb_func_nocIs_t)callback)(
				   event_data->nocIs->class_data
				   , event_data->nocIs->obj
				   , event_data->nocIs->bigint
				   , event_data->nocIs->string
				   );
    else
      (*(cb_func_nocIs_t2)callback)(
				    (void *) curr->obj
				    , event_data->nocIs->class_data
				    , event_data->nocIs->obj
				    , event_data->nocIs->bigint
				    , event_data->nocIs->string
				    );

    break;
    // nocs : class_data, conf_object_t, char*
  case QEMU_xterm_break_string:
  case QEMU_gfx_break_string:
    if (!curr->obj)
      (*(cb_func_nocs_t)callback)(
				  event_data->nocs->class_data
				  , event_data->nocs->obj
				  , event_data->nocs->string
				  );
    else
      (*(cb_func_nocs_t2)callback)(
				   (void *) curr->obj
				   , event_data->nocs->class_data
				   , event_data->nocs->obj
				   , event_data->nocs->string
				   );

    break;
    // ncm : conf_object_t, memory_transaction_t
  case QEMU_cpu_mem_trans:
#ifdef CONFIG_DEBUG_LIBQFLEX
      QEMU_increment_debug_stat(CPUMEMTRANS);
 #endif
      if (!curr->obj)
      (*(cb_func_ncm_t)callback)(
				 event_data->ncm->space
				 , event_data->ncm->trans
				 );
    else
      (*(cb_func_ncm_t2)callback)(
				  (void *) curr->obj
				  , event_data->ncm->space
				  , event_data->ncm->trans
				  );

    break;

  case QEMU_dma_mem_trans:
    if (!curr->obj){
      (*(cb_func_ncm_t)callback)(
				 event_data->ncm->space
				 , event_data->ncm->trans
				 );
    }
    else
      {
	//  printf("is this run(ifobj)\n");
	(*(cb_func_ncm_t2)callback)(
				    (void *) curr->obj
				    , event_data->ncm->space
				    , event_data->ncm->trans
				    );
      }
    break;
  default:
#ifdef CONFIG_DEBUG_LIBQFLEX
       QEMU_increment_debug_stat(NON_EXISTING_EVENT);
#endif
    break;
  }
}

void QEMU_execute_callbacks(
			       int cpu_id,
			       QEMU_callback_event_t event,
			       QEMU_callback_args_t *event_data) {

  QEMU_callback_table_t * generic_table = &QEMU_all_callbacks_tables[0];
  QEMU_callback_table_t * table = &QEMU_all_callbacks_tables[cpu_id+1];
  QEMU_callback_container_t *curr = table->callbacks[event];

  // execute specified callbacks
  for (; curr != NULL; curr = curr->next){
#ifdef CONFIG_DEBUG_LIBQFLEX
      QEMU_increment_debug_stat(ALL_CALLBACKS);
#endif
      do_execute_callback(curr, event, event_data);
    }
  if( cpu_id + 1 != 0 ) {
    // only execudebug_types::te the generic callbacks once
    curr = generic_table->callbacks[event];
    for (; curr != NULL; curr = curr->next){
#ifdef CONFIG_DEBUG_LIBQFLEX
        QEMU_increment_debug_stat(ALL_GENERIC_CALLBACKS);
#endif
        do_execute_callback(curr, event, event_data);
    }
  }
}
void QEMU_cpu_set_quantum(const int * val)
{
#ifdef CONFIG_QUANTUM
    /* Msutherl:
     * - I believe this should be removed, b/c the
     *   quantum is always set from QEMU cmd line.
     * - Removed for now.
     * - IF we want to keep it, use the interface in
     *   qemu's cpus.c file to set:
     *      quantum_state.quantum_value = *val
     */
    fprintf(stderr, "----- ERROR in: %s:%d, called QEMU_cpu_set_quantum after functionality removed.-----\n",__FILE__,__LINE__);
    exit(-1);
    /*
    if (*val > 0)
        quantum_value = *val;
    */
#endif
}
#ifdef __cplusplus
}
#endif