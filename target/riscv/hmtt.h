#ifndef RISCV_HMTT_H
#define RISCV_HMTT_H

// #include "cpu.h"


typedef struct HMTTState {
    uint64_t index;
    uint64_t size;
    uint64_t *cacheline;
    char *file;
} HMTTState;


typedef struct CPUArchState CPURISCVState;


int hmtt_update_cacheline(CPURISCVState *env, uint64_t addr, uint64_t pc, int type);

#endif