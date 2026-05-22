#pragma once
#include <stdint.h>
#define SYS_MEM_STATUS   0x01
#define SYS_FS_READ      0x10
#define SYS_FS_WRITE     0x11
#define SYS_AUDIO_PLAY   0x20
#define SYS_DISPLAY_DRAW 0x30
#define SYS_NET_FETCH    0x40
void     syscall_init(void);
uint64_t syscall_handle(uint64_t num, uint64_t a, uint64_t b, uint64_t c);
