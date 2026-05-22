#pragma once
#include <stdint.h>
void idt_init(void);
void idt_register_handler(uint8_t vec, void (*handler)(uint8_t, void*), void *ctx);
void idt_unregister_handler(uint8_t vec);
void idt_dispatch(uint8_t vec, uint64_t error_code);
