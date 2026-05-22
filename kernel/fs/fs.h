#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { bool available; uint64_t total_bytes,free_bytes; uint32_t root_entry_count,aion_memory_file_count; } FsInfo;
void   fs_init(void);
int    fs_read_file(const char *path, char *buf, int buf_sz);
int    fs_write_file_usb(const char *path, const char *data, int len);
bool   fs_file_exists(const char *path);
int    fs_mkdir_usb(const char *path);
FsInfo fs_get_info_usb(void);
FsInfo fs_get_info_ssd(void);
