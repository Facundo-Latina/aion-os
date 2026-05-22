#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t width,height; uint8_t *data; uint32_t data_len; } CamFrame;
void      cam_init(void);
CamFrame *cam_get_latest_frame(void);
bool      cam_is_available(void);
