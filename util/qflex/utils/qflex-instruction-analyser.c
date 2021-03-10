#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

static inline uint32_t extract32(uint32_t value, int start, int length)
{
    assert(start >= 0 && length > 0 && length <= 32 - start);
    return (value >> start) & (~0U >> (32 - length));
}

/* L1; C3.1 A64 instruction index by encoding */
typedef enum {
    DATA_PROC_IM,  /* Data processing - immediate */
    B_EXC_SYS,     /* Branch, exception generation and system insns */
    LDST,          /* Loads and stores */
    DATA_PROC_REG, /* Data processing - register */
    DATA_PROC_SIMD_FP, /* Data processing - SIMD and floating point */
    L1H_NR
} L1H;

/* L2H */
typedef enum {
/* Data processing - immediate */
    DP_IMM_PC,  /* PC-rel. addressing */
    DP_IMM_AS,  /* Add/subtract (immediate) */
    DP_IMM_LOG, /* Logical (immediate) */
    DP_IMM_MOV, /* Move wide (immediate) */
    DP_IMM_BIT, /* Bitfield */
    DP_IMM_EXT,  /* Extract */
//} DATA_PROC_IM_t;

//typedef enum {
/* Branch, exception generation and system insns */
    BRES_UNCONDITIONAL_BR_IMM, /* Unconditional branch (immediate) */
    BRES_COMPARE_N_BR_IMM,     /* Compare & branch (immediate) */
    BRES_TEST_N_BR_IMM,        /* Test & branch (immediate) */
    BRES_CONDITIONAL_BR_IMM,   /* Conditional branch (immediate) */
    BRES_SYSTEM_HINT,          /* System HINT */
    BRES_SYSTEM_CLREX,         /* System CLREX, DSB, DMB, ISB */
    BRES_SYSTEM_MSR,           /* System MSR (immediate) */
    BRES_SYSTEM_OTHER,          /* System (Other) */
    BRES_EXECP,                /* Exception generation */
    BRES_UNCONDITIONAL_BR_REG, /* Unconditional branch (register) */
//} B_EXC_SYS_t;

//typedef enum {
/* Loads and stores */
    LDST_L2H,
//} LDST_t;

//typedef enum {
/* Data processing - register */
    DP_REG_LOG_SR,     /* Logical (shifted register) */
    DP_REG_AS_EXT_REG, /* Add/subtract (extended register) */
    DP_REG_AS_REG,     /* Add/subtract (non extended register) */
    DP_REG_AS_CARRY,   /* Add/subtract (with carry) */
    DP_REG_COND_COMP,  /* Conditional compare */
    DP_REG_COMP_SELE,  /* Conditional select */
    DP_REG_DATA_1S,    /* Data-processing (1 source) */
    DP_REG_DATA_2S,    /* Data-processing (2 source) */
    DP_REG_DATA_3S,    /* Data-processing (3 source) */
//} DATA_PROC_REG_t;
/* Data processing - SIMD and floating point */
    // DP_SIMD_FP
    L2H_NR
} L2H;

/* LDST */
typedef enum {
    LDST_EXCLUSIVE, /* Load/store exclusive */
    LDST_REG_LIT,   /* Load register (literal) */
    LDST_SIMD_MULT, /* AdvSIMD load/store multiple structures */
    LDST_SIMD_SING, /* AdvSIMD load/store single structure */
/* Load/store register (all forms) */
    LDST_REG_IMM_UNSCALED, /* Load/store register (unscaled immediate)     */
    LDST_REG_UNPRIVILEGED, /* Load/store register (unprivileged)           */
    LDST_REG_IMM_POST_IDX, /* Load/store register (immediate post-indexed) */
    LDST_REG_IMM_PRE_IDX,  /* Load/store register (immediate pre-indexed)  */
    LDST_REG_ROFFSET,      /* Load/store register (register offset)        */
    LDST_REG_UNSIGNED_IMM, /* Load/store register (unsigned immediate)     */
    LDST_REG_SIMD,         /* Load/store register (SIMD)                   */
/* Load/store pair (all forms) */
    LDST_PAIR_NO_ALLOCATE_OFFSET, /* Load/store no-allocate pair (offset)    */
    LDST_PAIR_POST_IDX,           /* Load/store register pair (post-indexed) */
    LDST_PAIR_PRE_IDX,            /* Load/store register pair (offset)       */
    LDST_PAIR_OFFSET,             /* Load/store register pair (pre-indexed)  */
    LDST_PAIR_SIMD,               /* Load/store register pair (SIMD) */
    LDST_NR
} LDST_t;

typedef struct disas_profile_t {
    L1H l1h;
    L2H l2h; // L2H
    LDST_t ldst;
} disas_profile_t;

typedef struct QFLEXProfileItem {
    int enumerator;
    int enumerator_l1h;
    const char *name;
} QFLEXProfileItem;

typedef struct qflex_profile_t {
    long global_profiles_l1h[L1H_NR];
    long global_profiles_l2h[L2H_NR];
    long global_ldst[LDST_NR];
} qflex_profile_t;

extern qflex_profile_t qflex_profile_stats;

const char* qflex_profile_get_string_l1h(L1H l1h);
const char* qflex_profile_get_string_l2h(L2H l2h);
const char* qflex_profile_get_string_ldst(LDST_t ldst);
void qflex_profile_log_l2h_names_csv(void);
void qflex_profile_curr_el_log_stats_verbose(void);
void qflex_profile_curr_el_log_stats_csv(int el);
void qflex_profile_curr_el_reset(void);
void qflex_profile_global_log_stats(void);
void qflex_profile_global_reset(void);

static const QFLEXProfileItem l1h_items[] = {
    { DATA_PROC_IM, DATA_PROC_IM, "DATA_PROC_IM" },               /* Data processing - immediate */
    { B_EXC_SYS, B_EXC_SYS, "B_EXC_SYS" },                        /* Branch, exception generation and system insns */
    { LDST, LDST, "LDST" },                                       /* Loads and stores */
    { DATA_PROC_REG, DATA_PROC_REG, "DATA_PROC_REG" },            /* Data processing - register */
    { DATA_PROC_SIMD_FP, DATA_PROC_SIMD_FP,"DATA_PROC_SIMD_FP" }  /* Data processing - SIMD and floating point */
};

static const QFLEXProfileItem l2h_items[] = {
    /* Data processing - immediate */
    { DP_IMM_PC,  DATA_PROC_IM, "DP_IMM_PC"  }, /* PC-rel. addressing */
    { DP_IMM_AS,  DATA_PROC_IM, "DP_IMM_AS"  }, /* Add/subtract (immediate) */
    { DP_IMM_LOG, DATA_PROC_IM, "DP_IMM_LOG" }, /* Logical (immediate) */
    { DP_IMM_MOV, DATA_PROC_IM, "DP_IMM_MOV" }, /* Move wide (immediate) */
    { DP_IMM_BIT, DATA_PROC_IM, "DP_IMM_BIT" }, /* Bitfield */
    { DP_IMM_EXT, DATA_PROC_IM, "DP_IMM_EXT" }, /* Extract */
    /* Branch, exception generation and system insns */
    { BRES_UNCONDITIONAL_BR_IMM, B_EXC_SYS, "BRES_UNCOND_BR_IMM" }, /* Unconditional branch (immediate) */
    { BRES_COMPARE_N_BR_IMM,     B_EXC_SYS, "BRES_COMP_N_BR_IMM" }, /* Compare & branch (immediate) */
    { BRES_TEST_N_BR_IMM,        B_EXC_SYS, "BRES_TEST_N_BR_IMM" }, /* Test & branch (immediate) */
    { BRES_CONDITIONAL_BR_IMM,   B_EXC_SYS, "BRES_COND_BR_IMM"   }, /* Conditional branch (immediate) */
    { BRES_SYSTEM_HINT,          B_EXC_SYS, "BRES_SYSTEM_HINT"   }, /* System */
    { BRES_SYSTEM_CLREX,         B_EXC_SYS, "BRES_SYSTEM_CLREX"  }, /* System */
    { BRES_SYSTEM_MSR,           B_EXC_SYS, "BRES_SYSTEM_MSR"    }, /* System */
    { BRES_SYSTEM_OTHER,         B_EXC_SYS, "BRES_SYSTEM_OTHER"  }, /* System */
    { BRES_EXECP,                B_EXC_SYS, "BRES_EXECP"         }, /* Exception generation */
    { BRES_UNCONDITIONAL_BR_REG, B_EXC_SYS, "BRES_UNCOND_BR_REG" }, /* Unconditional branch (register) */
    /* Loads and stores */
    { LDST, LDST_L2H, "LDST_L2H" }, /* Load/store (all forms) */
    /* Data processing - register */
    { DP_REG_LOG_SR,     DATA_PROC_REG, "DP_REG_LOG_SR"     }, /* Logical (shifted register) */
    { DP_REG_AS_EXT_REG, DATA_PROC_REG, "DP_REG_AS_EXT_REG" }, /* Add/subtract (extended register) */
    { DP_REG_AS_REG,     DATA_PROC_REG, "DP_REG_AS_REG"     }, /* Add/subtract (non extended register) */
    { DP_REG_AS_CARRY,   DATA_PROC_REG, "DP_REG_AS_CARRY"   }, /* Add/subtract (with carry) */
    { DP_REG_COND_COMP,  DATA_PROC_REG, "DP_REG_COND_COMP"  }, /* Conditional compare */
    { DP_REG_COMP_SELE,  DATA_PROC_REG, "DP_REG_COMP_SELE"  }, /* Conditional select */
    { DP_REG_DATA_1S,    DATA_PROC_REG, "DP_REG_DATA_1S"    }, /* Data-processing (1 source) */
    { DP_REG_DATA_2S,    DATA_PROC_REG, "DP_REG_DATA_2S"    }, /* Data-processing (2 source) */
    { DP_REG_DATA_3S,    DATA_PROC_REG, "DP_REG_DATA_3S"    }, /* Data-processing (3 source) */
};

static const QFLEXProfileItem ldst_items[] = {
    { LDST_EXCLUSIVE, LDST, "LDST_EXCLUSIVE" }, /* Load/store exclusive */
    { LDST_REG_LIT,   LDST, "LDST_REG_LIT"   }, /* Load register (literal) */
    { LDST_SIMD_MULT, LDST, "LDST_SIMD_MULT" }, /* AdvSIMD load/store multiple structures */
    { LDST_SIMD_SING, LDST, "LDST_SIMD_SING" }, /* AdvSIMD load/store single structure */
/* Load/store register (all forms) */
    { LDST_REG_IMM_UNSCALED, LDST, "LDST_REG_IMM_UNSCALED" }, /* Load/store register (unscaled immediate)     */
    { LDST_REG_UNPRIVILEGED, LDST, "LDST_REG_UNPRIVILEGED" }, /* Load/store register (unprivileged)           */
    { LDST_REG_IMM_POST_IDX, LDST, "LDST_REG_IMM_POST_IDX" }, /* Load/store register (immediate post-indexed) */
    { LDST_REG_IMM_PRE_IDX,  LDST, "LDST_REG_IMM_PRE_IDX"  }, /* Load/store register (immediate pre-indexed)  */
    { LDST_REG_ROFFSET,      LDST, "LDST_REG_ROFFSET"      }, /* Load/store register (register offset)        */
    { LDST_REG_UNSIGNED_IMM, LDST, "LDST_REG_UNSIGNED_IMM" }, /* Load/store register (unsigned immediate)     */
    { LDST_REG_SIMD,         LDST, "LDST_REG_SIMD"         }, /* Load/store register (SIMD)                   */
/* Load/store pair (all forms) */
    { LDST_PAIR_NO_ALLOCATE_OFFSET, LDST, "LDST_PAIR_NO_ALLOCATE_OFFSET" }, /* Load/store no-allocate pair (offset)    */
    { LDST_PAIR_POST_IDX,    LDST, "LDST_PAIR_POST_IDX"    }, /* Load/store register pair (post-indexed) */
    { LDST_PAIR_PRE_IDX,     LDST, "LDST_PAIR_PRE_IDX"     }, /* Load/store register pair (offset)       */
    { LDST_PAIR_OFFSET,      LDST, "LDST_PAIR_OFFSET"      }, /* Load/store register pair (pre-indexed)  */
    { LDST_PAIR_SIMD,        LDST, "LDST_PAIR_SIMD"        }, /* Load/store register pair (SIMD) */
    { LDST_REG_IMM_POST_IDX, LDST, "LDST_REG_IMM_POST_IDX" }  /* Data processing - immediate */
};

qflex_profile_t qflex_profile_stats = {
    .global_profiles_l1h = {},
    .global_profiles_l2h = {},
    .global_ldst = {}
};

const char* qflex_profile_get_string_l1h(L1H l1h) {
    const char* name = "UNDEF";
    if(l1h < L1H_NR) {
        name = l1h_items[l1h].name;
    }
    return name;
}

const char* qflex_profile_get_string_l2h(L2H l2h) {
    const char* name = "UNDEF";
    if(l2h < L2H_NR) {
        name = l2h_items[l2h].name;
    }
    return name;
}

const char* qflex_profile_get_string_ldst(LDST_t ldst) {
    const char* name = "UNDEF";
    if(ldst < LDST_NR) {
        name = ldst_items[ldst].name;
    }
    return name;
}

void qflex_profile_log_l2h_names_csv(void) {
    for(int l2h = 0; l2h < L2H_NR; l2h++) {
        printf("%s,",qflex_profile_get_string_l2h(l2h));
    }
    printf("\n");
}

void qflex_profile_global_log_stats(void) {
    int l1h = 0;
        for(int l2h = 0; l2h < L2H_NR; l2h++) {
            l1h = l2h_items[l2h].enumerator_l1h;
            if (l2h == DP_IMM_PC ||
                l2h == BRES_UNCONDITIONAL_BR_IMM ||
                l2h == LDST_EXCLUSIVE ||
                l2h == DP_REG_LOG_SR  ||
                l2h == L2H_NR) {
                printf("  [%02i]:%-18s   :%ld\n", l1h, qflex_profile_get_string_l1h(l1h), qflex_profile_stats.global_profiles_l1h[l1h]);
            }
            printf("  - [%02i]:%-18s:%ld\n", l2h, qflex_profile_get_string_l2h(l2h), qflex_profile_stats.global_profiles_l2h[l2h]);
        }
    printf("LDST Details\n");
    for(int ldst = 0; ldst < LDST_NR; ldst++) {
        printf("[%02i]:%-18s:%ld\n", ldst, qflex_profile_get_string_ldst(ldst), qflex_profile_stats.global_ldst[ldst]);
    }
}

void qflex_profile_global_reset(void) {
        for(int l1h = 0; l1h < L1H_NR; l1h++) {
            qflex_profile_stats.global_profiles_l1h[l1h] = 0;
        }
        for(int l2h = 0; l2h < L2H_NR; l2h++) {
            qflex_profile_stats.global_profiles_l2h[l2h] = 0;
        }
    for(int ldst = 0; ldst < LDST_NR; ldst++) {
        qflex_profile_stats.global_ldst[ldst] = 0;
    }
}

static disas_profile_t insn_profile;

/* LDST:
 * Load/store (immediate post-indexed)
 * Load/store (immediate pre-indexed)
 * Load/store (unscaled immediate)
 *
 * 31 30 29   27  26 25 24 23 22 21  20    12 11 10 9    5 4    0
 * +----+-------+---+-----+-----+---+--------+-----+------+------+
 * |size| 1 1 1 | V | 0 0 | opc | 0 |  imm9  | idx |  Rn  |  Rt  |
 * +----+-------+---+-----+-----+---+--------+-----+------+------+
 *
 * idx = 01 -> post-indexed, 11 pre-indexed, 00 unscaled imm. (no writeback)
         10 -> unprivileged
 * V = 0 -> non-vector
 * size: 00 -> 8 bit, 01 -> 16 bit, 10 -> 32 bit, 11 -> 64bit
 * opc: 00 -> store, 01 -> loadu, 10 -> loads 64, 11 -> loads 32
 */
static void disas_ldst_reg_imm9(uint32_t insn)
{
    int idx = extract32(insn, 10, 2);
    bool is_vector = extract32(insn, 26, 1);
    if(!is_vector) {
        switch(idx) {
        case 0: insn_profile.ldst = LDST_REG_IMM_UNSCALED; break;
        case 1: insn_profile.ldst = LDST_REG_IMM_POST_IDX; break;
        case 2: insn_profile.ldst = LDST_REG_UNPRIVILEGED; break;
        case 3: insn_profile.ldst = LDST_REG_IMM_PRE_IDX; break;
        }
    } else {
        insn_profile.ldst = LDST_REG_SIMD;
    }
}

/* LDST: Load/store register (all forms) */
static void disas_ldst_reg(uint32_t insn)
{
    switch (extract32(insn, 24, 2)) {
    case 0:
        if (extract32(insn, 21, 1) == 1 && extract32(insn, 10, 2) == 2) {
            /* Load/store register (register offset) */
            insn_profile.ldst = LDST_REG_ROFFSET;
            // disas_ldst_reg_roffset(s, insn, opc, size, rt, is_vector);
        } else {
            /* Load/store register (unscaled immediate)
             * Load/store register (immediate pre/post-indexed)
             * Load/store register (unprivileged)
             */
            disas_ldst_reg_imm9(insn);
        }
        break;
    case 1:
        /* Load/store register (unsigned immediate) */
        insn_profile.ldst = LDST_REG_UNSIGNED_IMM;
        // disas_ldst_reg_unsigned_imm(s, insn, opc, size, rt, is_vector);
        break;
    default:
        // unallocated_encoding(s);
        break;
    }
}

/*
 * LDNP (Load Pair - non-temporal hint)
 * LDP (Load Pair - non vector)
 * LDPSW (Load Pair Signed Word - non vector)
 * STNP (Store Pair - non-temporal hint)
 * STP (Store Pair - non vector)
 * LDNP (Load Pair of SIMD&FP - non-temporal hint)
 * LDP (Load Pair of SIMD&FP)
 * STNP (Store Pair of SIMD&FP - non-temporal hint)
 * STP (Store Pair of SIMD&FP)
 *
 *  31 30 29   27  26  25 24   23  22 21   15 14   10 9    5 4    0
 * +-----+-------+---+---+-------+---+-----------------------------+
 * | opc | 1 0 1 | V | 0 | index | L |  imm7 |  Rt2  |  Rn  | Rt   |
 * +-----+-------+---+---+-------+---+-------+-------+------+------+
 *
 * opc: LDP/STP/LDNP/STNP        00 -> 32 bit, 10 -> 64 bit
 *      LDPSW                    01
 *      LDP/STP/LDNP/STNP (SIMD) 00 -> 32 bit, 01 -> 64 bit, 10 -> 128 bit
 *   V: 0 -> GPR, 1 -> Vector
 * idx: 00 -> signed offset with non-temporal hint, 01 -> post-index,
 *      10 -> signed offset, 11 -> pre-index
 *   L: 0 -> Store 1 -> Load
 *
 * Rt, Rt2 = GPR or SIMD registers to be stored
 * Rn = general purpose register containing address
 * imm7 = signed offset (multiple of 4 or 8 depending on size)
 */
static void disas_ldst_pair(uint32_t insn)
{
    int index = extract32(insn, 23, 2);
    bool is_vector = extract32(insn, 26, 1);

    if (!is_vector) {
        switch(index) {
        case 0: insn_profile.ldst = LDST_PAIR_NO_ALLOCATE_OFFSET; break;
        case 1: insn_profile.ldst = LDST_PAIR_POST_IDX; break;
        case 2: insn_profile.ldst = LDST_PAIR_PRE_IDX; break;
        case 3: insn_profile.ldst = LDST_PAIR_OFFSET; break;
        }
    } else {
        insn_profile.ldst = LDST_PAIR_SIMD;
    }
}

/* LDST: Data processing - register */
static void disas_data_proc_reg(uint32_t insn)
{
    switch (extract32(insn, 24, 5)) {
    case 0x0a: /* Logical (shifted register) */
        insn_profile.l2h = DP_REG_LOG_SR;
        // disas_logic_reg(s, insn);
        break;
    case 0x0b:
        if (insn & (1 << 21)) { /* Add/subtract (extended register) */
            insn_profile.l2h = DP_REG_AS_EXT_REG;
            // disas_add_sub_ext_reg(s, insn);
        } else {                /* Add/subtract (shifted register) */
            insn_profile.l2h = DP_REG_AS_REG;
            // disas_add_sub_reg(s, insn);
        }
        break;
    case 0x1b: /* Data-processing (3 source) */
        insn_profile.l2h = DP_REG_DATA_3S;
        // disas_data_proc_3src(s, insn);
        break;
    case 0x1a:
        switch (extract32(insn, 21, 3)) {
        case 0x0: /* Add/subtract (with carry) */
            insn_profile.l2h = DP_REG_AS_CARRY;
            // disas_adc_sbc(s, insn);
            break;
        case 0x2: /* Conditional compare */
            insn_profile.l2h = DP_REG_COND_COMP;
            // disas_cc(s, insn); /* both imm and reg forms */
            break;
        case 0x4: /* Conditional select */
            insn_profile.l2h = DP_REG_COMP_SELE;
            // disas_cond_select(s, insn);
            break;
        case 0x6: /* Data-processing */
            if (insn & (1 << 30)) { /** (1 source) */
                insn_profile.l2h = DP_REG_DATA_1S;
                // disas_data_proc_1src(s, insn);
            } else {                /** (2 source) */
                insn_profile.l2h = DP_REG_DATA_2S;
                // disas_data_proc_2src(s, insn);
            }
            break;
        default:
            // unallocated_encoding(s);
            break;
        }
        break;
    default:
        // unallocated_encoding(s);
        break;
    }
}

/* L2; Loads and stores */
static void disas_ldst(uint32_t insn)
{
    insn_profile.l2h = LDST_L2H;
    switch (extract32(insn, 24, 6)) {
    case 0x08: /* Load/store exclusive */
        insn_profile.ldst = LDST_EXCLUSIVE;
        // disas_ldst_excl(s, insn);
        break;
    case 0x18: case 0x1c: /* Load register (literal) */
        insn_profile.ldst = LDST_REG_LIT;
        // disas_ld_lit(s, insn);
        break;
    case 0x28: case 0x29:
    case 0x2c: case 0x2d: /* Load/store pair (all forms) */
        disas_ldst_pair(insn);
        break;
    case 0x38: case 0x39:
    case 0x3c: case 0x3d: /* Load/store register (all forms) */
        disas_ldst_reg(insn);
        break;
    case 0x0c: /* AdvSIMD load/store multiple structures */
        insn_profile.ldst = LDST_SIMD_MULT;
        // disas_ldst_multiple_struct(s, insn);
        break;
    case 0x0d: /* AdvSIMD load/store single structure */
        insn_profile.ldst = LDST_SIMD_SING;
        // disas_ldst_single_struct(s, insn);
        break;
    default:
        // unallocated_encoding(s);
        break;
    }
}

/* System
 *  31                 22 21  20 19 18 16 15   12 11    8 7   5 4    0
 * +---------------------+---+-----+-----+-------+-------+-----+------+
 * | 1 1 0 1 0 1 0 1 0 0 | L | op0 | op1 |  CRn  |  CRm  | op2 |  Rt  |
 * +---------------------+---+-----+-----+-------+-------+-----+------+
 */
static void disas_system(uint32_t insn)
{
    unsigned int l, op0, op1, crn, crm, op2, rt;
    l = extract32(insn, 21, 1);
    op0 = extract32(insn, 19, 2);
    op1 = extract32(insn, 16, 3);
    crn = extract32(insn, 12, 4);
    crm = extract32(insn, 8, 4);
    op2 = extract32(insn, 5, 3);
    rt = extract32(insn, 0, 5);

    if (op0 == 0) {
        if (l || rt != 31) {
            //unallocated_encoding(s);
            return;
        }
        switch (crn) {
        case 2: /* HINT (including allocated hints like NOP, YIELD, etc) */
            //handle_hint(s, insn, op1, op2, crm);
            insn_profile.l2h = BRES_SYSTEM_HINT;
            break;
        case 3: /* CLREX, DSB, DMB, ISB */
            insn_profile.l2h = BRES_SYSTEM_CLREX;
            //handle_sync(s, insn, op1, op2, crm);
            break;
        case 4: /* MSR (immediate) */
            insn_profile.l2h = BRES_SYSTEM_MSR;
            //handle_msr_i(s, insn, op1, op2, crm);
            break;
        default:
            //unallocated_encoding();
            break;
        }
        return;
    }
    insn_profile.l2h = BRES_SYSTEM_OTHER;
}

/* Branch, exception generation and system insns */
/* L2; Branches, exception generating and system instructions */
static void disas_b_exc_sys(uint32_t insn)
{
    switch (extract32(insn, 25, 7)) {
    case 0x0a: case 0x0b:
    case 0x4a: case 0x4b: /* Unconditional branch (immediate) */
        insn_profile.l2h = BRES_UNCONDITIONAL_BR_IMM;
        // disas_uncond_b_imm(s, insn);
        break;
    case 0x1a: case 0x5a: /* Compare & branch (immediate) */
        insn_profile.l2h = BRES_COMPARE_N_BR_IMM;
        // disas_comp_b_imm(s, insn);
        break;
    case 0x1b: case 0x5b: /* Test & branch (immediate) */
        insn_profile.l2h = BRES_TEST_N_BR_IMM;
        // disas_test_b_imm(s, insn);
        break;
    case 0x2a: /* Conditional branch (immediate) */
        insn_profile.l2h = BRES_CONDITIONAL_BR_IMM;
        // disas_cond_b_imm(s, insn);
        break;
    case 0x6a: /* Exception generation */
        if (insn & (1 << 24)) {
            //insn_profile.l2h = BRES_SYSTEM;
            disas_system(insn);
        } else {
            insn_profile.l2h = BRES_EXECP;
            // disas_exc(s, insn);
        }
        break;
    case 0x6b: /* Unconditional branch (register) */
        insn_profile.l2h = BRES_UNCONDITIONAL_BR_REG;
        // disas_uncond_b_reg(s, insn);
        break;
    default:
        // unallocated_encoding(s);
        break;
    }
}


/* L2; Data processing - immediate */
static void disas_data_proc_imm(uint32_t insn)
{
    switch (extract32(insn, 23, 6)) {
    case 0x20: case 0x21: /* PC-rel. addressing */
        insn_profile.l2h = DP_IMM_PC;
        // disas_pc_rel_adr(s, insn);
        break;
    case 0x22: case 0x23: /* Add/subtract (immediate) */
        insn_profile.l2h = DP_IMM_AS;
        // disas_add_sub_imm(s, insn);
        break;
    case 0x24: /* Logical (immediate) */
        insn_profile.l2h = DP_IMM_LOG;
        // disas_logic_imm(s, insn);
        break;
    case 0x25: /* Move wide (immediate) */
        insn_profile.l2h = DP_IMM_MOV;
        // disas_movw_imm(s, insn);
        break;
    case 0x26: /* Bitfield */
        insn_profile.l2h = DP_IMM_BIT;
        // disas_bitfield(s, insn);
        break;
    case 0x27: /* Extract */
        insn_profile.l2h = DP_IMM_EXT;
        // disas_extract(s, insn);
        break;
    default:
        // unallocated_encoding(s);
        break;
    }
}

/* L1; C3.1 A64 instruction index by encoding */
static void qflex_profile_disas_a64_insn(uint32_t insn)
{
    insn_profile.l1h = L1H_NR;
    insn_profile.l2h = L2H_NR;
    insn_profile.ldst = LDST_NR;

    switch (extract32(insn, 25, 4)) {
    case 0x0: case 0x1: case 0x2: case 0x3: /* UNALLOCATED */
        // unallocated_encoding(s);
        break;
    case 0x8: case 0x9: /* Data processing - immediate */
        insn_profile.l1h = DATA_PROC_IM;
        disas_data_proc_imm(insn);
        break;
    case 0xa: case 0xb: /* Branch, exception generation and system insns */
        insn_profile.l1h = B_EXC_SYS;
        disas_b_exc_sys(insn);
        break;
    case 0x4: case 0x6: case 0xc: case 0xe:      /* Loads and stores */
        insn_profile.l1h = LDST;
        disas_ldst(insn);
        break;
    case 0x5: case 0xd:      /* Data processing - register */
        insn_profile.l1h = DATA_PROC_REG;
        disas_data_proc_reg(insn);
        break;
    case 0x7: case 0xf:      /* Data processing - SIMD and floating point */
        insn_profile.l1h = DATA_PROC_SIMD_FP;
        // disas_data_proc_simd_fp(s, insn);
        break;
    default:
        //assert(FALSE); /* all 15 cases should be handled above */
        break;
    }
    long l1h = insn_profile.l1h;
    long l2h = insn_profile.l2h;
    long ldst = insn_profile.ldst;

    qflex_profile_stats.global_profiles_l1h[l1h]++;
    qflex_profile_stats.global_profiles_l2h[l2h]++;
    if(l1h == LDST) { qflex_profile_stats.global_ldst[ldst]++; }

}

#define PATH_MAX 4096
#define ROOT_DIR "/tmp/qflex"
int main(int argc, char *argv[]) { 
	//const char *filename = argv[1];
	int core_count = atoi(argv[1]);
	if(core_count>64) exit(1);
	
	char filepath[PATH_MAX];
	char filename[sizeof "inst_trace_00"];
	FILE **files = calloc(core_count, sizeof(FILE*));
	int available = 0;

 	for(int i = 0; i < core_count; i++) { 
		sprintf(filename, "inst_trace_%02d", i);
      	snprintf(filepath, PATH_MAX, ROOT_DIR"/%s", filename);
		files[i] = fopen(filepath, "r");
		available |= 1 << i;
	}

    int rdSize = 0;
    uint32_t inst;
    long tot_insts = 0;
	while (available) {
	    for(int cpu = 0; cpu < core_count; cpu++) {
			if(available & 1 << cpu) {
				rdSize = fread(&inst, sizeof(uint32_t), 1, files[cpu]);
				if(rdSize != 1) {
					available &= ~(1 << cpu);
					fclose(files[cpu]);
				} else {
                    qflex_profile_disas_a64_insn(inst);
                    tot_insts++;
                }
			}
	    }
    }
    printf("Total insts: %zu", tot_insts);
    qflex_profile_global_log_stats();
	return 0;
}