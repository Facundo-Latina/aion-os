#pragma once
#include <stdint.h>
typedef struct { uint16_t vendor,device; uint8_t class_,subclass; uint8_t bus,slot,func; uint32_t bar[6]; uint8_t irq; } PciDevice;
void     pci_init(void);
int      pci_find_device(uint16_t vendor, uint16_t device, PciDevice *out);
int      pci_find_class(uint8_t class_, uint8_t subclass, PciDevice *out);
uint32_t pci_read32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off);
void     pci_write32(uint8_t bus, uint8_t slot, uint8_t func, uint8_t off, uint32_t v);
uint64_t pci_get_bar(PciDevice *dev, int idx);
void     pci_enable_bus_master(PciDevice *dev);
