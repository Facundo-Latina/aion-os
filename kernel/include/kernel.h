#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
void kernel_main(void *boot_info);
void kernel_panic(const char *msg);
