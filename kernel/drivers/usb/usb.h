#pragma once
#include <stdint.h>
#include <stdbool.h>

void usb_init(void);
bool usb_device_connected(uint8_t port);
