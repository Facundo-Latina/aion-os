#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct {
    uint64_t total_physical;
    uint64_t free_physical;
    uint64_t used_physical;
    uint64_t swap_total;
    uint64_t swap_used;
    int      pressure_level;
} MemoryStatus;
void         memory_init(uint64_t mmap_addr, uint64_t mmap_size, uint64_t desc_size);
uint64_t     pmm_alloc(void);
uint64_t     pmm_alloc_contiguous(uint64_t n);
void         pmm_free(uint64_t phys_addr);
void        *kmalloc(uint64_t size);
void        *kmalloc_aligned(uint64_t size, uint64_t align);
void         kfree(void *ptr);
MemoryStatus memory_get_status(void);
