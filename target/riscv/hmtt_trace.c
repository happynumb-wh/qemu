#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "cpu.h"
#include "exec/icount.h"
#include "hmtt.h"
#include "cpu.h"

#define RING_BUFFER_SIZE 512//(0x800000 / 64)  // size of ring buffer

uint64_t gb_store_counter = 0;
uint64_t gb_load_counter = 0;

int record_switch = 1;

typedef struct {
    uint64_t r_ret;
    uint64_t w_ret;
    uint64_t addr_r;
    uint64_t addr_w;
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
    data->addr_r = rb->buffer[rb->tail].addr_r;
    data->addr_w = rb->buffer[rb->tail].addr_w;
    data->r_ret = rb->buffer[rb->tail].r_ret;
    data->w_ret = rb->buffer[rb->tail].w_ret;
    data->ptr = rb->buffer[rb->tail].ptr;
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    rb->count--;
    return true;
}

// get a trace item from trace buffer
static void get_item_from_trace(cache_record_t * item)
{

    if (hmtt_state.size == hmtt_state.index)
    {
        hmtt_state.size = 0;
        hmtt_state.index = 0;
        if (trace_end)
            hmtt_end = 1;

        while (hmtt_state.size == 0)
        {
            fill_hmtt_trace();
            // hmtt_state.trace_ptr += (MAX_TRACE_SIZE * sizeof(HMTTTraceEntry));
        }
        
        if (record_flag) {
            printf("end to record\n");
            if (record_flag && record_fp)
            {
                fclose(record_fp);
                record_fp = NULL;
                record_flag = 0;
            }
        }
    }

    item->addr_r = hmtt_state.trace_cacheline[hmtt_state.index].addr_r;
    item->addr_w = hmtt_state.trace_cacheline[hmtt_state.index].addr_w;
    item->r_ret = hmtt_state.trace_cacheline[hmtt_state.index].r_ret;
    item->w_ret = hmtt_state.trace_cacheline[hmtt_state.index].w_ret;
    hmtt_state.index++;
    item->ptr = hmtt_state.trace_ptr;
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

    while (1)
    {
        get_item_from_trace(&pop_addr);
        gb_load_counter =  pop_addr.r_ret;
        gb_store_counter = pop_addr.w_ret;
        
        // if ((gb_load_counter + gb_store_counter) >= 20000000000UL && (gb_load_counter + gb_store_counter) <= 20100000000UL)
        if ((gb_load_counter + gb_store_counter) <= 100000000UL)
        {
            if (pop_addr.addr_r)
            {
                // if (pop_addr.r_ret)
                    fprintf(record_log_fp, "%s,0x%lx, pc: 0x%lx, l: %ld, s: %ld\n","R", pop_addr.addr_r, pc, pop_addr.r_ret, pop_addr.w_ret);
                // fprintf(record_log_fp, "l: %ld, s: %ld\n", pop_addr.r_ret, pop_addr.w_ret);
            }

            if (pop_addr.addr_w)
            {
                // if (pop_addr.w_ret)
                    fprintf(record_log_fp, "%s,0x%lx, pc: 0x%lx, l: %ld, s: %ld\n","W", pop_addr.addr_w, pc, pop_addr.r_ret, pop_addr.w_ret);
                // fprintf(record_log_fp, "l: %ld, s: %ld\n", pop_addr.r_ret, pop_addr.w_ret);
            }

            fflush(record_log_fp);
        }

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
