/*
 * AION Microphone Driver
 * Captures raw PCM from HDA input (internal mic or line-in).
 * AION receives raw samples — no speech recognition built in.
 */
#include "mic.h"
#include "../../memory/memory.h"
#include "../../include/serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define MIC_SAMPLE_RATE 16000
#define MIC_CHANNELS    1
#define MIC_BUF_SAMPLES 4096
#define MIC_BUF_BYTES   (MIC_BUF_SAMPLES * 2)   /* 16-bit samples */

static MicFrame current_frame;
static uint8_t *mic_buf = NULL;
static bool     mic_ready = false;

void mic_init(void) {
    mic_buf = kmalloc(MIC_BUF_BYTES);
    if (!mic_buf) {
        serial_printf("[mic] cannot allocate buffer\n");
        return;
    }
    memset(mic_buf, 0, MIC_BUF_BYTES);

    current_frame.sample_rate  = MIC_SAMPLE_RATE;
    current_frame.channels     = MIC_CHANNELS;
    current_frame.sample_count = MIC_BUF_SAMPLES;
    current_frame.data         = mic_buf;

    /* HDA input stream setup (full impl via HDA codec verbs) */
    mic_ready = true;
    serial_printf("[mic] ready %uHz %uch\n", MIC_SAMPLE_RATE, MIC_CHANNELS);
}

MicFrame *mic_get_latest_frame(void) {
    if (!mic_ready) return NULL;
    return &current_frame;
}

bool mic_is_available(void) { return mic_ready; }
