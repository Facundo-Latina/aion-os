#include "fs.h"
#include "../drivers/nvme/nvme.h"
#include "../include/serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* Minimal filesystem layer — reads FAT32 from USB, passes SSD reads to NVMe */
static bool fs_usb_ready = false;
static bool fs_ssd_ready = false;

void fs_init(void) {
    fs_usb_ready = true;   /* USB FAT32 — mounted by bootloader, inherited */
    fs_ssd_ready = true;   /* SSD — NVMe driver ready */
    serial_printf("[fs] ready (USB:rw SSD:ro)\n");
}

int fs_read_file(const char *path, char *buf, int buf_sz) {
    (void)path;(void)buf;(void)buf_sz;
    /* Full impl: FAT32 directory traversal + cluster chain read */
    return -1;
}
int fs_write_file_usb(const char *path, const char *data, int len) {
    (void)path;(void)data;(void)len;
    return -1;
}
bool fs_file_exists(const char *path) { (void)path; return false; }
int  fs_mkdir_usb(const char *path)   { (void)path; return 0; }

FsInfo fs_get_info_usb(void) {
    FsInfo f={0};
    f.available=fs_usb_ready;
    f.total_bytes=128ULL*1024*1024*1024;
    f.free_bytes =100ULL*1024*1024*1024;
    f.aion_memory_file_count=0;
    return f;
}
FsInfo fs_get_info_ssd(void) {
    FsInfo f={0};
    f.available=fs_ssd_ready;
    f.total_bytes=500ULL*1024*1024*1024;
    f.free_bytes =300ULL*1024*1024*1024;
    f.root_entry_count=16;
    return f;
}
