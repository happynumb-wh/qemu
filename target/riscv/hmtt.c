#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "cpu.h"
#include "exec/icount.h"
#include "hmtt.h"
#include "cpu.h"
#include <elf.h>
HMTTState hmtt_state;
FILE *hmtt_trace_fp = NULL;
int trace_end = 0;
int hmtt_end = 0;
HMTTTraceEntry * trace_buffer = NULL;
uint64_t trace_file_size = 0;


uint64_t global_num = 0;
int trace_buffer_ok = 0;
pthread_mutex_t buffer_lock;
pthread_t thread_id;
int record_flag = 0;
const char * record_file = "hmtt_record.txt";
const char * record_log_file = "hmtt_record.log";
FILE * record_fp = NULL;
FILE * record_log_fp = NULL;


elf_info_t elf_info;

static void * thread_trace_buffer(void *arg) {
    while (1) {
        pthread_mutex_lock(&buffer_lock);
        if (trace_buffer_ok == 0) {
            global_num = fread(trace_buffer, TRACE_LENGTH, MAX_TRACE_SIZE, hmtt_trace_fp);
            trace_file_size += (global_num * TRACE_LENGTH);
            printf("Read thread alone %ld entries from trace file, %ld GB\n", global_num, trace_file_size / (1024 * 1024 * 1024));
            trace_buffer_ok = 1;
            if (global_num < MAX_TRACE_SIZE)
            {
                trace_end = 1;
                pthread_mutex_unlock(&buffer_lock);
                return NULL;
            }
                        
        }
        pthread_mutex_unlock(&buffer_lock);
    }
    return NULL;
}

static void init_elf_info(elf_info_t *elf_info, const char *elf_file)
{
    FILE *fp = fopen(elf_file, "rb");
    assert(fp != NULL);

    // Read ELF header
    Elf64_Ehdr ehdr;
    assert(fread(&ehdr, 1, sizeof(ehdr), fp) == sizeof(ehdr));

    // Read section headers to find .text section
    fseek(fp, ehdr.e_shoff, SEEK_SET);
    elf_info->text_start = __INT64_MAX__;
    elf_info->text_end = 0;
    for (int i = 0; i < ehdr.e_shnum; i++) {
        Elf64_Shdr shdr;
        assert(fread(&shdr, 1, sizeof(shdr), fp) == sizeof(shdr));
        if (shdr.sh_type == SHT_PROGBITS && (shdr.sh_flags & SHF_EXECINSTR)) {
            if (shdr.sh_addr < elf_info->text_start)
                elf_info->text_start = shdr.sh_addr;
            if (shdr.sh_addr + shdr.sh_size > elf_info->text_end)
                elf_info->text_end = shdr.sh_addr + shdr.sh_size;
        }
    }

    fclose(fp);
}


// Initialize HMTT state
void init_hmtt_state(void)
{
    hmtt_state.file = strdup(hmtt_trace_file);
    hmtt_state.index = 0;
    hmtt_state.size = 0;
    hmtt_trace_fp = fopen(hmtt_state.file, "r");
    assert(hmtt_trace_fp != NULL);
    fseek(hmtt_trace_fp, 0, SEEK_SET);

    // Init cacheline buffer
    hmtt_state.cacheline = malloc(CACHELINE_SIZE);
    assert(hmtt_state.cacheline != NULL);
    memset(hmtt_state.cacheline, 0, CACHELINE_SIZE);
    hmtt_state.trace_cacheline = malloc(MAX_TRACE_SIZE * sizeof(HMTTTraceEntry));
    assert(hmtt_state.trace_cacheline != NULL);
    trace_buffer = malloc(MAX_TRACE_SIZE * sizeof(HMTTTraceEntry));
    assert(trace_buffer != NULL);
    hmtt_state.trace_ptr = 0;

    record_fp = fopen(record_file, "w");
    assert(record_fp != NULL);
    record_log_fp = fopen(record_log_file, "w");
    assert(record_log_fp != NULL);

    if (hmtt_elf_file)
    {
        init_elf_info(&elf_info, hmtt_elf_file);
        printf("ELF text section: 0x%lx - 0x%lx\n", elf_info.text_start, elf_info.text_end);
    }

    assert(pthread_mutex_init(&buffer_lock, NULL) == 0);
    pthread_create(&thread_id, NULL, thread_trace_buffer, NULL);
}

static uint64_t addr_check(uint64_t addr)
{

    uint64_t offset_addr;
    if (addr == 0) return -1;
    if (addr < PHYS_MEM_BASE || addr >= (PHYS_MEM_BASE + PHYS_MEM_SIZE))
    {
        offset_addr = addr;
    } else {
        offset_addr = addr - PHYS_MEM_BASE + HMTT_ADDR_LOW;
    }

    // if (offset_addr >= elf_info.text_start && offset_addr < elf_info.text_end)
    // {
    //     return -1;
    // }

    return offset_addr;
}


void get_trace_from_file(void)
{
    uint64_t num;
wait:
    pthread_mutex_lock(&buffer_lock);
    if (trace_buffer_ok == 0) {
        pthread_mutex_unlock(&buffer_lock);
        goto wait;
    }

    num = global_num;
    // trace_file_size += (num * TRACE_LENGTH);
    // printf("Read %ld entries from trace file, total size: %ld GB\n", num, trace_file_size / (1024 * 1024 * 1024));
    assert(num > 0);
    hmtt_state.trace_ptr += (MAX_TRACE_SIZE * sizeof(HMTTTraceEntry));
    for (int i = 0; i < num; i++)
    {
        uint64_t addr_r = (trace_buffer[i].addr_r & 0x7fffffff) << 5;
        uint64_t addr_w = (trace_buffer[i].addr_w & 0x7fffffff) << 5;
        int save_r = 0;
        int save_w = 0;

        addr_r = addr_check(addr_r);
        
        if (addr_r != (uint64_t)-1)
            save_r = 1;
        
        addr_w = addr_check(addr_w);
        if (addr_w != (uint64_t)-1)
            save_w = 1;

        if (!save_r && !save_w)
            continue;
        
        int index = hmtt_state.size++;
        hmtt_state.trace_cacheline[index].addr_r = 0;
        hmtt_state.trace_cacheline[index].addr_w = 0;
        hmtt_state.trace_cacheline[index].r_ret = trace_buffer[i].r_ret;
        hmtt_state.trace_cacheline[index].w_ret = trace_buffer[i].w_ret;
        if (save_r)
        {   
            assert((addr_r & 0x3f) == 0);
            hmtt_state.trace_cacheline[index].addr_r = addr_r;
        }
        if (save_w)
        {
            assert((addr_w & 0x3f) == 0);
            hmtt_state.trace_cacheline[index].addr_w = addr_w;
        }
    }
    trace_buffer_ok = 0;
    pthread_mutex_unlock(&buffer_lock);
}


void fill_hmtt_trace(void)
{
    get_trace_from_file();
}



// int map_ok = 0;
int hmtt_forward(CPURISCVState *env, uint64_t addr, uint64_t pc, int type)
{
    // uint64_t cache_address = addr & ~0x3f;
    // uint64_t cache_index = CACHELINE_INDEX(cache_address);
    if (hmtt_end) return -1;

        
    // if (!map_ok)
    // {
    //     init_trace_buffer();
    //     map_ok = 1;
    // } 
    // special
    update_hmtt_trace(env, pc, addr, type);

    return 0;
}


int hmtt_update_memtrace(CPURISCVState *env, uint64_t addr, uint64_t pc, int type)
{
    if (hmtt_end) return -1;

    if (hmtt_trace_file == NULL)
    {
        return -1;
    }

    if (hmtt_trace_fp == NULL && hmtt_trace_file)
    {
        // Init hmtt_state
        init_hmtt_state();
    }

    if ((gb_load_counter + gb_store_counter) >= 20000000000UL && (gb_load_counter + gb_store_counter) <= 20100000000UL)
    // if ((gb_load_counter + gb_store_counter) <= 100000000UL)
    {
        fprintf(record_fp, "0x%lx,%s,0x%lx,l: %ld,s: %ld\n", pc, type == 1 ? "W" : "R", addr, env->hmttloadinstrs, env->hmttstoreinstrs);
        // fprintf(record_fp, "l: %ld, s: %ld\n", env->hmttloadinstrs, env->hmttstoreinstrs);
        fflush(record_fp);
    }


    if (type == LOAD && env->hmttloadinstrs < gb_load_counter)
    {
        return -1;
    }

    if (type == STORE && env->hmttstoreinstrs < gb_store_counter)
    {
        return -1;
    }


    if (type == LOAD && CACHELINE_INDEX(addr) == CACHELINE_INDEX(gb_load_addr) && (env->hmttloadinstrs - gb_load_counter) < 16)
    {
        fprintf(stderr, "LOAD miss: pc: 0x%lx, addr: 0x%lx, l: %ld, s: %ld\n", pc, addr, env->hmttloadinstrs, env->hmttstoreinstrs);
    }

    return hmtt_forward(env, addr, pc, type);
}