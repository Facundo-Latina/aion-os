#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t sample_rate,channels,sample_count; uint8_t *data; } MicFrame;
void      mic_init(void);
MicFrame *mic_get_latest_frame(void);
bool      mic_is_available(void);
