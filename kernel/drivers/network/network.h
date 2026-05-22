#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { bool initialised,link_up; uint64_t rx_bytes,tx_bytes; } NetworkStatus;
void          network_init(void);
NetworkStatus network_get_status(void);
int           network_http_get(const char *url, char *buf, int buf_sz);
