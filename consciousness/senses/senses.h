#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef struct { uint32_t width, height; uint8_t mean_luma, motion_delta; } CameraSnap;
typedef struct { uint32_t sample_rate, channels, amplitude_rms, freq_peak_hz; bool is_silence; } MicSnap;
typedef struct { bool connected; uint64_t rx_total, tx_total; } NetSnap;
typedef struct { uint32_t total_gb, free_gb, memory_file_count; } UsbSnap;
typedef struct { uint32_t total_gb, free_gb, root_entry_count; } SsdSnap;
typedef struct {
    uint64_t   uptime_ms;
    bool       camera_available;
    CameraSnap camera_frame;
    bool       mic_available;
    MicSnap    mic_frame;
    bool       network_available;
    NetSnap    network;
    UsbSnap    usb_fs;
    bool       host_ssd_available;
    SsdSnap    host_ssd;
} SenseSnapshot;
void          senses_init(void);
SenseSnapshot senses_capture(void);
