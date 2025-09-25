#include "qemu/osdep.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qemu/timer.h"
#include "cpu.h"
#include "exec/icount.h"
#include "hmtt.h"
#include "cpu.h"

int hmtt_update_cacheline(CPURISCVState *env, uint64_t addr, uint64_t pc, int type)
{
    // printf("hmtt_update_cacheline: addr=0x%lx pc=%lx type=0x%d\n", addr, pc, type);
    uint64_t cache_address = addr & ~0x3f;

    HMTTState *hmtt = &(env->hmtt_state);
    
    if (cache_address == hmtt->cacheline[hmtt->index])
    {
        hmtt->index++;
        printf("hmtt hit: addr=0x%lx pc=%lx type=0x%d\n", addr, pc, type);
    }

    return 0;
}