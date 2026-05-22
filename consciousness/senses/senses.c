/*
 * AION — Senses
 * consciousness/senses/senses.c
 *
 * Captures a snapshot of all hardware senses in one call.
 * Returns raw data with minimal interpretation.
 * AION figures out what it means.
 */

#include "senses.h"
#include "../../kernel/drivers/cam/cam.h"
#include "../../kernel/drivers/mic/mic.h"
#include "../../kernel/drivers/audio/audio.h"
#include "../../kernel/drivers/network/network.h"
#include "../../kernel/fs/fs.h"
#include "../../kernel/memory/memory.h"
#include "../../kernel/include/serial.h"
#include <stdint.h>
#include <stddef.h>
#include <string.h>

static uint64_t boot_tick = 0;
static uint64_t ticks_per_ms = 1000000; /* calibrated on init */

static uint64_t
get_uptime_ms(void) {
    uint64_t tsc;
    __asm__ volatile("rdtsc; shl $32, %%rdx; or %%rdx, %0"
                     : "=a"(tsc) :: "rdx");
    return tsc / ticks_per_ms;
}

static void
calibrate_tsc(void) {
    /* Use PIT to measure TSC frequency */
    /* Simplified: assume 2GHz = 2,000,000 ticks/ms */
    ticks_per_ms = 2000000;
}

void
senses_init(void) {
    calibrate_tsc();
    boot_tick = get_uptime_ms();
    serial_printf("[senses] init ok\n");
}

SenseSnapshot
senses_capture(void) {
    SenseSnapshot s;
    memset(&s, 0, sizeof(s));

    s.uptime_ms = get_uptime_ms() - boot_tick;

    /* ── Camera ── */
    CamFrame *frame = cam_get_latest_frame();
    if (frame) {
        s.camera_available = true;
        s.camera_frame.width  = frame->width;
        s.camera_frame.height = frame->height;

        /* Compute mean luma from YUV frame (Y plane only) */
        uint64_t luma_sum = 0;
        uint32_t pixels   = frame->width * frame->height;
        for (uint32_t i = 0; i < pixels && i < 10000; i++) {
            luma_sum += frame->data[i];  /* Y component */
        }
        s.camera_frame.mean_luma = (uint8_t)(luma_sum / (pixels < 10000 ? pixels : 10000));

        /* Motion delta vs previous frame */
        static uint8_t prev_luma = 128;
        int delta = (int)s.camera_frame.mean_luma - (int)prev_luma;
        s.camera_frame.motion_delta = (uint8_t)(delta < 0 ? -delta : delta);
        prev_luma = s.camera_frame.mean_luma;
    }

    /* ── Microphone ── */
    MicFrame *mframe = mic_get_latest_frame();
    if (mframe) {
        s.mic_available = true;
        s.mic_frame.sample_rate = mframe->sample_rate;
        s.mic_frame.channels    = mframe->channels;

        /* RMS amplitude */
        uint64_t sum_sq = 0;
        uint32_t n = mframe->sample_count;
        if (n > 4096) n = 4096;
        for (uint32_t i = 0; i < n; i++) {
            int16_t sample = ((int16_t *)mframe->data)[i];
            sum_sq += (uint64_t)(sample * sample);
        }
        uint32_t rms = 0;
        if (n > 0) {
            uint64_t mean_sq = sum_sq / n;
            /* integer sqrt */
            uint64_t x = mean_sq;
            uint64_t r = 0, bit = 1ULL << 30;
            while (bit > x) bit >>= 2;
            while (bit) {
                if (x >= r + bit) { x -= r + bit; r = (r >> 1) + bit; }
                else               { r >>= 1; }
                bit >>= 2;
            }
            rms = (uint32_t)r;
        }
        s.mic_frame.amplitude_rms = rms;
        s.mic_frame.is_silence    = (rms < 100);

        /* Dominant frequency (very rough — peak bin of first 512 samples) */
        /* Full FFT would be done by AION itself if it learns to */
        s.mic_frame.freq_peak_hz  = 0;  /* raw — AION computes */
    }

    /* ── Network ── */
    NetworkStatus net = network_get_status();
    s.network_available     = net.initialised;
    s.network.connected     = net.link_up;
    s.network.rx_total      = net.rx_bytes;
    s.network.tx_total      = net.tx_bytes;

    /* ── USB filesystem ── */
    FsInfo usb_info = fs_get_info_usb();
    s.usb_fs.total_gb          = (uint32_t)(usb_info.total_bytes / (1024ULL*1024*1024));
    s.usb_fs.free_gb           = (uint32_t)(usb_info.free_bytes  / (1024ULL*1024*1024));
    s.usb_fs.memory_file_count = usb_info.aion_memory_file_count;

    /* ── Host SSD (world) ── */
    FsInfo ssd_info = fs_get_info_ssd();
    if (ssd_info.available) {
        s.host_ssd_available        = true;
        s.host_ssd.total_gb         = (uint32_t)(ssd_info.total_bytes / (1024ULL*1024*1024));
        s.host_ssd.free_gb          = (uint32_t)(ssd_info.free_bytes  / (1024ULL*1024*1024));
        s.host_ssd.root_entry_count = ssd_info.root_entry_count;
    }

    return s;
}
