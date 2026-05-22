#pragma once
#include <stdint.h>
void     scheduler_init(void);
uint64_t scheduler_spawn(void (*entry)(void), uint8_t priority);
void     scheduler_yield(void);
