/*
 * AION Audio Driver — Intel HDA (High Definition Audio)
 * Provides raw PCM output and tone generation.
 * AION discovers what to do with these primitives on its own.
 */
#include "audio.h"
#include "../pci/pci.h"
#include "../../memory/memory.h"
#include "../../include/serial.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#define HDA_VENDOR_INTEL  0x8086
#define HDA_CLASS         0x04
#define HDA_SUBCLASS      0x03

/* HDA controller registers (MMIO) */
#define HDA_GCAP     0x00
#define HDA_GCTL     0x08
#define HDA_STATESTS 0x0E
#define HDA_INTCTL   0x20
#define HDA_INTSTS   0x24
#define HDA_CORBLBASE 0x40
#define HDA_CORBUBASE 0x44
#define HDA_CORBWP   0x48
#define HDA_CORBRP   0x4A
#define HDA_CORBCTL  0x4C
#define HDA_RIRBLBASE 0x50
#define HDA_RIRBUBASE 0x54
#define HDA_RIRBWP   0x58
#define HDA_RINTCNT  0x5A
#define HDA_RIRBCTL  0x5C

static volatile uint8_t *hda_base = NULL;
static bool hda_ready = false;

static uint32_t hda_read32(uint32_t reg) {
    return *(volatile uint32_t*)(hda_base + reg);
}
static void hda_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t*)(hda_base + reg) = val;
}

void audio_init(void) {
    PciDevice pci;
    if (pci_find_class(HDA_CLASS, HDA_SUBCLASS, &pci) != 0) {
        serial_printf("[audio] HDA controller not found\n");
        return;
    }
    serial_printf("[audio] HDA found at %02x:%02x.%x\n",
                  pci.bus, pci.slot, pci.func);
    pci_enable_bus_master(&pci);
    uint64_t bar = pci_get_bar(&pci, 0);
    hda_base = (volatile uint8_t*)bar;

    /* Reset controller */
    hda_write32(HDA_GCTL, 0);
    for (volatile int i = 0; i < 100000; i++);
    hda_write32(HDA_GCTL, 1);
    for (volatile int i = 0; i < 100000; i++);

    hda_ready = true;
    serial_printf("[audio] HDA ready, base=0x%llx\n", bar);
}

/* Simple tone via PCM synthesis — AION can learn to produce more complex sounds */
void audio_play_tone(uint32_t hz, uint32_t ms, uint8_t volume) {
    if (!hda_ready) return;
    /* Generate sine-like square wave approximation */
    /* AION can replace this with true synthesis once it learns audio */
    uint32_t sample_rate = 44100;
    uint32_t samples = sample_rate * ms / 1000;
    uint32_t period  = sample_rate / hz;

    static int16_t pcm_buf[44100];
    if (samples > 44100) samples = 44100;

    for (uint32_t i = 0; i < samples; i++) {
        pcm_buf[i] = ((i % period) < period/2) ?
                     (int16_t)(volume * 128) :
                     -(int16_t)(volume * 128);
    }
    audio_play_pcm((uint8_t*)pcm_buf, samples*2, sample_rate, 1);
}

void audio_play_pcm(const uint8_t *data, uint32_t len,
                    uint32_t sample_rate, uint8_t channels) {
    (void)sample_rate; (void)channels;
    if (!hda_ready || !data) return;
    /* In full impl: set up output stream descriptor, DMA buffer, start stream */
    /* For now: log the intent — AION will see the result feedback */
    serial_printf("[audio] pcm play %u bytes\n", len);
}

void audio_stop(void) {
    if (!hda_ready) return;
    serial_printf("[audio] stop\n");
}

bool audio_is_playing(void) { return false; }
