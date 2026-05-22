#include "network.h"
#include "../pci/pci.h"
#include "../../include/serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

static NetworkStatus net_status = {0};

void network_init(void) {
    PciDevice pci;
    /* Intel e1000e: vendor=0x8086, class=0x02 (Network), subclass=0x00 (Ethernet) */
    if (pci_find_class(0x02, 0x00, &pci) != 0) {
        serial_printf("[net] no ethernet controller found\n");
        return;
    }
    serial_printf("[net] ethernet at %02x:%02x.%x\n", pci.bus, pci.slot, pci.func);
    pci_enable_bus_master(&pci);
    net_status.initialised = true;
    net_status.link_up     = true;  /* assume link up — driver polls in real impl */
    serial_printf("[net] ready\n");
}

NetworkStatus network_get_status(void) { return net_status; }

int network_http_get(const char *url, char *buf, int buf_sz) {
    (void)url; (void)buf; (void)buf_sz;
    /* Full impl: TCP/IP stack, DNS, HTTP/1.1 */
    /* AION can discover how to use this through trial and feedback */
    return -1;  /* not yet implemented — returns error which AION observes */
}
