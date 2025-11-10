#ifndef RISCV_HMTT_H
#define RISCV_HMTT_H

#define PHYS_MEM_BASE       0x100000000UL
#define PHYS_MEM_SIZE       0x100000000UL


#define HMTT_ADDR_LOW            0x100000UL
#define HMTT_ADDR_HIGH           0x100100000UL


#define MAX_TRACE_SIZE      0x8000000
#define CACHELINE_SIZE      ((HMTT_ADDR_HIGH - HMTT_ADDR_LOW) / 64)
#define CACHELINE_MASK      0xf
#define CACHELINE_INDEX(addr)   (((addr - HMTT_ADDR_LOW) >> 6))

#define CACHELINE_VALID     (1UL << 0)
#define CACHELINE_DIRTY     (1UL << 1)
#define CACHELINE_ACCESSED  (1UL << 2)

#define TRACE_LENGTH      12

#define LOAD 0
#define STORE 1


typedef struct __attribute__((packed)) HMTTRawTraceEntry {
    uint64_t addr;
    uint16_t r_ret;
    uint16_t w_ret;
    // uint64_t ptr;
} HMTTRawTraceEntry;


typedef struct HMTTTraceEntry {
    uint64_t addr;
    uint8_t RW; // 0: read, 1: write
    uint8_t NE;
    uint8_t timer;
    uint16_t axi_id;
    uint16_t r_ret;
    uint16_t w_ret;
    // uint64_t ptr;
} HMTTTraceEntry;


typedef struct HMTTState {
    char * file;
    uint64_t index;
    uint64_t size;
    uint8_t *cacheline;
    HMTTTraceEntry *trace_cacheline;
    uint64_t trace_ptr;
} HMTTState;


typedef struct elf_info {
    uint64_t text_start;
    uint64_t text_end;
} elf_info_t;

#define CACHE_SIZE 512


extern HMTTState hmtt_state;
extern const char *hmtt_trace_file;
extern const char *hmtt_elf_file;


typedef struct CPUArchState CPURISCVState;

int hmtt_update_memtrace(CPURISCVState *env, uint64_t addr, uint64_t pc, int type);
void init_hmtt_state(void);
void fill_hmtt_trace(void);
void get_trace_from_file(void);
int update_hmtt_trace(CPURISCVState *env, uint64_t pc, uint64_t addr, int type);


int hmtt_forward(CPURISCVState *env, uint64_t addr, uint64_t pc, int type);

// Ring Buffer for trace addresses
extern int trace_buffer_ok;
extern HMTTRawTraceEntry * trace_buffer;
extern pthread_mutex_t buffer_lock;
extern uint64_t global_num;
extern uint64_t trace_file_size;
extern int record_flag;
extern const char * record_file;
extern const char * record_log_file;
extern FILE * record_log_fp;
extern FILE * record_fp;


extern uint64_t gb_store_counter;
extern uint64_t gb_load_counter;
extern uint64_t gb_load_addr;
extern uint64_t gb_store_addr;

extern int trace_end;
extern int hmtt_end;

void init_trace_buffer(void);

int update_cache_status(uint64_t addr, int type);

#endif