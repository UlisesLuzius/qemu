#ifndef SAVEVM_EXT_H
#define SAVEVM_EXT_H

#include <stdbool.h>

#include "qemu/osdep.h"


extern bool exton;
extern const char* loadext;
int extsnap_parse_opts(int index, const char *optarg, Error **errp);
void execute_extsnap(void);
void clean_extsnap(void);

int save_vmstate_ext(Monitor *mon, const char *name);
int save_vmstate_ext_test(Monitor *mon, const char *name);
int incremental_load_vmstate_ext(const char *name, Monitor* mon);
int create_tmp_overlay(void);
int delete_tmp_overlay(void);
uint64_t get_phase_value(void);
bool is_phases_enabled(void);
bool is_ckpt_enabled(void);
void toggle_phases_creation(void);
void toggle_ckpt_creation(void);
bool phase_is_valid(void);
void save_phase(void);
void save_ckpt(void);
void pop_phase(void);
bool save_request_pending(void);
bool cont_request_pending(void);
bool quit_request_pending(void);
void request_cont(void);
void request_quit(void);
void toggle_cont_request(void);
void toggle_save_request(void);
void set_base_ckpt_name(const char* str);
const char* get_ckpt_name(void);
uint64_t get_ckpt_interval(void);
uint64_t get_ckpt_end(void);
bool can_quit(void);
void toggle_can_quit(void);

#endif
