#pragma once
#include <stdint.h>
void     display_init(uint64_t fb, uint32_t w, uint32_t h, uint32_t pitch, uint32_t fmt);
void     display_set_pixel(uint32_t x, uint32_t y, uint32_t color);
void     display_clear(void);
void     display_print_string(const char *s, uint32_t color);
void     display_print_boot_line(const char *stage, const char *msg);
uint32_t display_get_width(void);
uint32_t display_get_height(void);
