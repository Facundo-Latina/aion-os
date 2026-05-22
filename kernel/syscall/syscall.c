#include "syscall.h"
#include "../memory/memory.h"
#include "../include/serial.h"
#include <stdint.h>
void syscall_init(void) {
    serial_printf("[syscall] init\n");
}
uint64_t syscall_handle(uint64_t num, uint64_t a, uint64_t b, uint64_t c) {
    (void)a;(void)b;(void)c;
    switch(num) {
        case SYS_MEM_STATUS: { MemoryStatus s=memory_get_status(); return s.free_physical; }
        default: return (uint64_t)-1;
    }
}
