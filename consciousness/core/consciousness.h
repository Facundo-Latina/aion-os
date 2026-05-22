#pragma once
#include <stdint.h>
void consciousness_init(void);
void consciousness_run(void);
uint64_t consciousness_get_tick(void);
const char *consciousness_get_context(void);
const char *consciousness_get_last_output(void);
