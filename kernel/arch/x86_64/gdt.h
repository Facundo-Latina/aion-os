#pragma once
#include <stdint.h>
void     gdt_init(void);
uint64_t gdt_get_tss_rsp0(void);
void     gdt_set_tss_rsp0(uint64_t rsp0);
