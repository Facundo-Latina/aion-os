#pragma once
#include <stdint.h>
void     nvme_init(void);
int      nvme_read(uint64_t lba, uint32_t count, void *buf);
int      nvme_write(uint64_t lba, uint32_t count, const void *buf);
uint64_t nvme_total_sectors(void);
