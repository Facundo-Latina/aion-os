#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct {
    uint64_t magic;
    uint64_t kernel_phys_addr;
    uint64_t kernel_size;
    uint64_t mmap_addr;
    uint64_t mmap_size;
    uint64_t mmap_desc_size;
    uint32_t mmap_desc_version;
    uint64_t mmap_key;
    uint64_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_format;
    uint64_t rsdp_addr;
    uint64_t reserved[8];
} __attribute__((packed)) AionBootInfo;

void kernel_main(AionBootInfo *boot_info);
void kernel_panic(const char *msg);
