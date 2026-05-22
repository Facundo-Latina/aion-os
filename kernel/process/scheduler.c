#include "scheduler.h"
#include "../include/serial.h"
#include <stdint.h>
void scheduler_init(void) { serial_printf("[sched] init (cooperative)\n"); }
uint64_t scheduler_spawn(void (*entry)(void), uint8_t priority) {
    (void)priority;
    /* Minimal: just call entry directly — AION is single-threaded for now */
    /* Real impl: proper task structs, context switching, priority queues */
    entry();
    return 0;
}
void scheduler_yield(void) { __asm__ volatile("hlt"); }
