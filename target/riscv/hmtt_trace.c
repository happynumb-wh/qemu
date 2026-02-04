#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "cpu.h"
#include "exec/icount.h"
#include "hmtt.h"
#include "cpu.h"

#define RING_BUFFER_SIZE 512//(0x800000 / 64)  // size of ring buffer

uint64_t gb_store_counter = 0x21a38de5c;
uint64_t gb_load_counter = 0x389a43aea;
uint64_t gb_load_addr = 0;
uint64_t gb_store_addr = 0;

int record_switch = 1;

typedef struct {
    uint64_t addr;
    uint64_t r_ret;
    uint64_t w_ret;
    uint8_t NE;
    uint8_t RW;
    uint8_t timer;
    uint16_t axi_id;
    uint64_t ptr;
} cache_record_t;

typedef struct {
    cache_record_t buffer[RING_BUFFER_SIZE];
    int threshold;
    int head;  // write pointer
    int tail;  // read pointer
    int count; // number
} RingBuffer;

RingBuffer cacheline_rb;

static void rb_init(RingBuffer *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
    rb->threshold = 0;
}

static bool rb_is_empty(RingBuffer *rb) {
    return rb->count == 0;
}

static bool rb_is_full(RingBuffer *rb) {
    return rb->count == RING_BUFFER_SIZE;
}

static bool rb_push(RingBuffer *rb, cache_record_t data) {
    if (rb_is_full(rb)) {
        return false;  // full buffer
    }
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % RING_BUFFER_SIZE;
    rb->count++;
    return true;
}

__attribute_maybe_unused__ static bool rb_pop(RingBuffer *rb, cache_record_t *data) {
    if (rb_is_empty(rb)) {
        return false;  // empty buffer
    }
    data->addr = rb->buffer[rb->tail].addr;
    data->r_ret = rb->buffer[rb->tail].r_ret;
    data->w_ret = rb->buffer[rb->tail].w_ret;
    data->ptr = rb->buffer[rb->tail].ptr;
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    rb->count--;
    return true;
}

// get a trace item from trace buffer
static int get_item_from_trace(cache_record_t * item)
{

    if (hmtt_state.size == hmtt_state.index)
    {
        hmtt_state.size = 0;
        hmtt_state.index = 0;
        if (trace_end)
        {
            // Just try to get trace
            fill_hmtt_trace();

            if (hmtt_state.size == 0)
            {
                hmtt_end = 1;
                return -1;                
            }
        }
            
        fill_hmtt_trace();
    
        if (hmtt_state.size == 0)
        {
            hmtt_end = 1;
            return -1;
        }
        
    }



    item->addr = hmtt_state.trace_cacheline[hmtt_state.index].addr;
    item->r_ret = (uint64_t)hmtt_state.trace_cacheline[hmtt_state.index].r_ret;
    item->w_ret = (uint64_t)hmtt_state.trace_cacheline[hmtt_state.index].w_ret;
    item->RW = hmtt_state.trace_cacheline[hmtt_state.index].RW;
    item->timer = hmtt_state.trace_cacheline[hmtt_state.index].timer;
    item->axi_id = hmtt_state.trace_cacheline[hmtt_state.index].axi_id;
    hmtt_state.index++;
    item->ptr = hmtt_state.trace_ptr;
    return 0;
}


void init_trace_buffer(void)
{
    rb_init(&cacheline_rb);

    while (!rb_is_full(&cacheline_rb))
    {
        cache_record_t item;
        get_item_from_trace(&item);
        rb_push(&cacheline_rb, item);
    }
}


int update_hmtt_trace(CPURISCVState *env, uint64_t pc, uint64_t addr, int type)
{
    if (record_flag)
    {
        fprintf(record_log_fp, "0x%lx hit\n", addr);
    }
    cache_record_t pop_addr = {0};
    // uint64_t cache_address = addr & ~0x3f;
    while (1)
    {
        if (get_item_from_trace(&pop_addr) == -1)
        {
            return -1;
        }
        gb_load_counter +=  pop_addr.r_ret;
        gb_store_counter += pop_addr.w_ret;

        if (pop_addr.RW == LOAD)
            gb_load_addr = pop_addr.addr;
        else
            gb_store_addr = pop_addr.addr;
        
        // if ((gb_load_counter + gb_store_counter) >= 10000000000UL && (gb_load_counter + gb_store_counter) <= 11000000000UL)
        // {
            if (pop_addr.addr)
            {
                fprintf(record_log_fp, "%s,0x%lx, pc: 0x%lx, l: %ld, s: %ld, axi_id: %d\n", pop_addr.RW == LOAD ? "R" : "W", pop_addr.addr, pc, gb_load_counter, gb_store_counter, pop_addr.axi_id); 
                fflush(record_log_fp);
            }
        // }

        // if (type == LOAD)
        // {
        //     if (pop_addr.RW == LOAD && pop_addr.addr == cache_address && gb_load_counter == env->hmttloadinstrs)
        //     {
        //         fprintf(stderr, "LOAD cache miss: 0x%lx, pc: 0x%lx, total load: %ld\n", addr, pc, gb_load_counter);
        //     }
        // }
        

        if ((type == LOAD && gb_load_counter > env->hmttloadinstrs) || \
            (type == STORE && gb_store_counter > env->hmttstoreinstrs))
        {
            return 0;
        }            
    }






    // while (rb_pop(&cacheline_rb, &pop_addr))
    // {
    //     gb_load_counter =  pop_addr.r_ret;
    //     gb_store_counter = pop_addr.w_ret;

    //     if ((type == LOAD && gb_load_counter > env->hmttloadinstrs) ||
    //         (type == STORE && gb_store_counter > env->hmttstoreinstrs))
    //     {
    //         break;
    //     }
    // }

    // while (!rb_is_full(&cacheline_rb))
    // {
    //     cache_record_t push_addr = {0};
    //     get_item_from_trace(&push_addr);
    //     rb_push(&cacheline_rb, push_addr);
    // }

    return 0;
}
