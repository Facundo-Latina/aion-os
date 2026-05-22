#include "pci.h"
#include "../../include/io.h"
#include "../../include/serial.h"
#include <stdint.h>
#define PCI_CONFIG_ADDR 0xCF8
#define PCI_CONFIG_DATA 0xCFC
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off) {
    uint32_t addr = 0x80000000|(bus<<16)|(slot<<11)|(func<<8)|(off&0xFC);
    outl(PCI_CONFIG_ADDR,addr);
    return inl(PCI_CONFIG_DATA);
}
void pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v) {
    uint32_t addr = 0x80000000|(bus<<16)|(slot<<11)|(func<<8)|(off&0xFC);
    outl(PCI_CONFIG_ADDR,addr);
    outl(PCI_CONFIG_DATA,v);
}
static void probe_device(uint8_t bus, uint8_t slot, uint8_t func) {
    uint32_t id=pci_read32(bus,slot,func,0);
    if(id==0xFFFFFFFF) return;
    uint32_t cc=pci_read32(bus,slot,func,8);
    serial_printf("[pci] %02x:%02x.%x vendor=%04x dev=%04x class=%02x:%02x\n",
        bus,slot,func,id&0xFFFF,id>>16,(cc>>24)&0xFF,(cc>>16)&0xFF);
}
void pci_init(void) {
    for(int bus=0;bus<256;bus++)
        for(int slot=0;slot<32;slot++)
            probe_device(bus,slot,0);
}
int pci_find_class(uint8_t class_, uint8_t subclass, PciDevice *out) {
    for(int bus=0;bus<256;bus++) for(int slot=0;slot<32;slot++) {
        uint32_t id=pci_read32(bus,slot,0,0);
        if(id==0xFFFFFFFF) continue;
        uint32_t cc=pci_read32(bus,slot,0,8);
        if(((cc>>24)&0xFF)==class_ && ((cc>>16)&0xFF)==subclass) {
            out->vendor=(uint16_t)(id&0xFFFF); out->device=(uint16_t)(id>>16);
            out->bus=bus; out->slot=slot; out->func=0;
            for(int b=0;b<6;b++) out->bar[b]=pci_read32(bus,slot,0,0x10+b*4);
            out->irq=(uint8_t)(pci_read32(bus,slot,0,0x3C)&0xFF);
            return 0;
        }
    }
    return -1;
}
int pci_find_device(uint16_t vendor, uint16_t device, PciDevice *out) {
    for(int bus=0;bus<256;bus++) for(int slot=0;slot<32;slot++) {
        uint32_t id=pci_read32(bus,slot,0,0);
        if((uint16_t)(id&0xFFFF)==vendor && (uint16_t)(id>>16)==device) {
            out->vendor=vendor; out->device=device;
            out->bus=bus; out->slot=slot; out->func=0;
            for(int b=0;b<6;b++) out->bar[b]=pci_read32(bus,slot,0,0x10+b*4);
            return 0;
        }
    }
    return -1;
}
void pci_enable_bus_master(PciDevice *dev) {
    uint32_t cmd=pci_read32(dev->bus,dev->slot,dev->func,4);
    pci_write32(dev->bus,dev->slot,dev->func,4,cmd|0x07);
}
uint64_t pci_get_bar(PciDevice *dev, int idx) {
    uint32_t lo=dev->bar[idx];
    if(lo&1) return lo&~3ULL; /* I/O */
    if((lo>>1&3)==2) {        /* 64-bit */
        uint64_t hi=dev->bar[idx+1];
        return (lo&~0xFULL)|(hi<<32);
    }
    return lo&~0xFULL;
}
