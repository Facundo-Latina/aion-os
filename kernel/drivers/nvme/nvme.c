#include "nvme.h"
#include "../pci/pci.h"
#include "../../memory/memory.h"
#include "../../include/serial.h"
#include <stdint.h>
/* NVMe: class=0x01, subclass=0x08 */
#define NVME_CLASS    0x01
#define NVME_SUBCLASS 0x08
static uint64_t nvme_base = 0;
static uint64_t total_sectors_val = 0;
void nvme_init(void) {
    PciDevice pci;
    if (pci_find_class(NVME_CLASS, NVME_SUBCLASS, &pci) != 0) {
        serial_printf("[nvme] no NVMe found\n");
        return;
    }
    nvme_base = pci_get_bar(&pci, 0);
    pci_enable_bus_master(&pci);
    total_sectors_val = 1024ULL*1024*1024*500/512; /* assume 500GB SSD */
    serial_printf("[nvme] ready base=0x%llx\n", nvme_base);
}
int nvme_read(uint64_t lba, uint32_t count, void *buf) {
    (void)lba;(void)count;(void)buf;
    return -1; /* full NVMe queue impl needed */
}
int nvme_write(uint64_t lba, uint32_t count, const void *buf) {
    (void)lba;(void)count;(void)buf;
    return -1;
}
uint64_t nvme_total_sectors(void) { return total_sectors_val; }
