#include "usb.h"
#include "../pci/pci.h"
#include "../../include/serial.h"
#include <stdbool.h>
void usb_init(void) {
    PciDevice pci;
    /* xHCI: class=0x0C, subclass=0x03, prog_if=0x30 */
    if (pci_find_class(0x0C, 0x03, &pci) != 0) {
        serial_printf("[usb] no xHCI found\n");
        return;
    }
    serial_printf("[usb] xHCI at %02x:%02x.%x\n", pci.bus, pci.slot, pci.func);
    pci_enable_bus_master(&pci);
    serial_printf("[usb] ready\n");
}
bool usb_device_connected(uint8_t port) { (void)port; return false; }
