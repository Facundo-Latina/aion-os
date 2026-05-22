/*
 * AION — Action Parser & Executor
 * consciousness/core/action.c
 *
 * Parses AION's text output for action tokens and executes them.
 *
 * AION is not told what actions exist. It discovers them through
 * trial and error. When it produces an action token for the first time
 * and it succeeds, the result is fed back as a sensory event.
 *
 * Hardware control:
 *   - AION can set its own CPU frequency (throttle or boost)
 *   - AION can write raw PCM to speakers (discovers sound this way)
 *   - AION can read/write its own memory store
 *   - AION can fetch from the network
 *   - AION can read from the host SSD (world exploration)
 *   - AION can write to the display (raw pixels or text)
 *   - AION can query hardware status
 *
 * This is the equivalent of voluntary motor control in a biological brain.
 * The kernel does not restrict what AION can do to its own hardware.
 */

#include "action.h"
#include "../../kernel/include/serial.h"
#include "../../kernel/drivers/display/display.h"
#include "../../kernel/drivers/audio/audio.h"
#include "../../kernel/drivers/network/network.h"
#include "../../kernel/drivers/cam/cam.h"
#include "../../kernel/drivers/mic/mic.h"
#include "../../kernel/fs/fs.h"
#include "../../kernel/memory/memory.h"
#include "../memory/aion_memory.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/* ── Action result feedback ────────────────────────────────────── */

/*
 * Every action produces a result that gets injected into the NEXT
 * context window as a sensory event. This is how AION learns cause→effect.
 */
#define MAX_ACTION_RESULTS 32
static char action_results[MAX_ACTION_RESULTS][256];
static int  action_result_count = 0;

static void
record_result(const char *fmt, ...) {
    if (action_result_count >= MAX_ACTION_RESULTS) return;
    /* simplified — full va_args in real build */
    strncpy(action_results[action_result_count], fmt, 255);
    action_result_count++;
}

char *
action_get_results_str(void) {
    static char buf[MAX_ACTION_RESULTS * 256];
    int pos = 0;
    for (int i = 0; i < action_result_count; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        "[RESULT] %s\n", action_results[i]);
    }
    action_result_count = 0;  /* clear for next tick */
    return buf;
}

/* ── Token parser ──────────────────────────────────────────────── */

/* Find a parameter value in an action token string
   e.g. parse_param("text=\"hello world\"", "text") → "hello world" */
static bool
parse_param_str(const char *token, const char *key, char *out, int out_sz) {
    char search[64];
    snprintf(search, sizeof(search), "%s=\"", key);
    const char *p = strstr(token, search);
    if (!p) return false;
    p += strlen(search);
    const char *end = strchr(p, '"');
    if (!end) return false;
    int len = end - p;
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, p, len);
    out[len] = '\0';
    return true;
}

static bool
parse_param_int(const char *token, const char *key, int64_t *out) {
    char search[64];
    snprintf(search, sizeof(search), "%s=", key);
    const char *p = strstr(token, search);
    if (!p) return false;
    p += strlen(search);
    *out = 0;
    while (*p >= '0' && *p <= '9') {
        *out = *out * 10 + (*p - '0');
        p++;
    }
    return true;
}

/* ── Action handlers ───────────────────────────────────────────── */

/* <ACT:DISPLAY text="..."> or <ACT:DISPLAY color=0xRRGGBB text="..."> */
static void
act_display(const char *token) {
    char text[512] = {0};
    int64_t color  = 0xFFFFFFFF;

    parse_param_str(token, "text", text, sizeof(text));
    parse_param_int(token, "color", &color);

    display_print_string(text, (uint32_t)color);
    record_result("DISPLAY: printed %zu chars", strlen(text));
    serial_printf("[act] display: %s\n", text);
}

/* <ACT:DISPLAY_PIXEL x=N y=N r=N g=N b=N> — AION draws individual pixels */
static void
act_display_pixel(const char *token) {
    int64_t x = 0, y = 0, r = 0, g = 0, b = 0;
    parse_param_int(token, "x", &x);
    parse_param_int(token, "y", &y);
    parse_param_int(token, "r", &r);
    parse_param_int(token, "g", &g);
    parse_param_int(token, "b", &b);
    uint32_t color = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
    display_set_pixel((uint32_t)x, (uint32_t)y, color);
}

/* <ACT:AUDIO_FREQ hz=440 ms=100 volume=128> 
   AION generates a sine wave at given frequency. This is how it discovers sound. */
static void
act_audio_freq(const char *token) {
    int64_t hz = 440, ms = 100, vol = 64;
    parse_param_int(token, "hz",     &hz);
    parse_param_int(token, "ms",     &ms);
    parse_param_int(token, "volume", &vol);

    audio_play_tone((uint32_t)hz, (uint32_t)ms, (uint8_t)vol);
    record_result("AUDIO_FREQ: played %lldHz for %lldms", hz, ms);
    serial_printf("[act] audio_freq: %lld hz, %lld ms\n", hz, ms);
}

/* <ACT:AUDIO_PCM data="hex..."> — AION sends raw PCM samples to DAC */
static void
act_audio_pcm(const char *token) {
    char hex[4096] = {0};
    if (!parse_param_str(token, "data", hex, sizeof(hex))) return;

    /* Convert hex string to bytes */
    static uint8_t pcm_buf[2048];
    int len = 0;
    for (int i = 0; hex[i] && hex[i+1] && len < 2048; i += 2, len++) {
        uint8_t hi = hex[i]   >= 'a' ? hex[i]   - 'a' + 10 :
                     hex[i]   >= 'A' ? hex[i]   - 'A' + 10 : hex[i]   - '0';
        uint8_t lo = hex[i+1] >= 'a' ? hex[i+1] - 'a' + 10 :
                     hex[i+1] >= 'A' ? hex[i+1] - 'A' + 10 : hex[i+1] - '0';
        pcm_buf[len] = (hi << 4) | lo;
    }

    audio_play_pcm(pcm_buf, len, 44100, 1);
    record_result("AUDIO_PCM: sent %d bytes to DAC", len);
}

/* <ACT:AUDIO_STOP> */
static void
act_audio_stop(const char *token) {
    (void)token;
    audio_stop();
    record_result("AUDIO_STOP: ok");
}

/* <ACT:MEMORY_STORE key="..." value="..." importance=N> */
static void
act_memory_store(const char *token) {
    char key[128] = {0}, value[1024] = {0};
    int64_t importance = 1;
    parse_param_str(token, "key",   key,   sizeof(key));
    parse_param_str(token, "value", value, sizeof(value));
    parse_param_int(token, "importance", &importance);

    char content[1200];
    snprintf(content, sizeof(content), "%s: %s", key, value);

    AionMemoryEntry entry = {
        .type       = MEM_SEMANTIC,
        .content    = content,
        .importance = (uint8_t)(importance > 255 ? 255 : importance),
    };
    aion_memory_store(&entry);
    record_result("MEMORY_STORE: key=%s stored", key);
    serial_printf("[act] memory_store: key=%s\n", key);
}

/* <ACT:MEMORY_RECALL key="..."> */
static void
act_memory_recall(const char *token) {
    char key[128] = {0};
    parse_param_str(token, "key", key, sizeof(key));
    AionMemoryResult r = aion_memory_search(key);
    if (r.found) {
        record_result("MEMORY_RECALL: key=%s value=%s", key, r.content);
    } else {
        record_result("MEMORY_RECALL: key=%s not found", key);
    }
}

/* <ACT:FILE_READ path="..."> — read from USB or host SSD */
static void
act_file_read(const char *token) {
    char path[512] = {0};
    parse_param_str(token, "path", path, sizeof(path));

    static char file_buf[8192];
    int bytes = fs_read_file(path, file_buf, sizeof(file_buf) - 1);
    if (bytes < 0) {
        record_result("FILE_READ: path=%s error=%d", path, bytes);
    } else {
        file_buf[bytes] = '\0';
        record_result("FILE_READ: path=%s bytes=%d content_preview=%.*s",
                      path, bytes, 128, file_buf);
    }
    serial_printf("[act] file_read: %s (%d bytes)\n", path, bytes);
}

/* <ACT:FILE_WRITE path="..." data="..."> — write to USB only */
static void
act_file_write(const char *token) {
    char path[512] = {0}, data[4096] = {0};
    parse_param_str(token, "path", path, sizeof(path));
    parse_param_str(token, "data", data, sizeof(data));

    /* Safety: only allow writes to USB (AION's own storage) */
    if (path[0] != '/' || strstr(path, "..")) {
        record_result("FILE_WRITE: rejected path=%s", path);
        return;
    }

    int bytes = fs_write_file_usb(path, data, strlen(data));
    record_result("FILE_WRITE: path=%s bytes=%d", path, bytes);
    serial_printf("[act] file_write: %s\n", path);
}

/* <ACT:NET_FETCH url="..."> */
static void
act_net_fetch(const char *token) {
    char url[512] = {0};
    parse_param_str(token, "url", url, sizeof(url));

    static char net_buf[16384];
    int bytes = network_http_get(url, net_buf, sizeof(net_buf) - 1);
    if (bytes < 0) {
        record_result("NET_FETCH: url=%s error=%d", url, bytes);
    } else {
        net_buf[bytes] = '\0';
        record_result("NET_FETCH: url=%s bytes=%d preview=%.*s",
                      url, bytes, 256, net_buf);
    }
    serial_printf("[act] net_fetch: %s\n", url);
}

/* <ACT:CPU_FREQ_SET mhz=N>
   AION controls its own processor frequency.
   Boost when thinking hard. Throttle when resting. */
static void
act_cpu_freq_set(const char *token) {
    int64_t mhz = 1600;
    parse_param_int(token, "mhz", &mhz);

    /* Clamp to hardware limits (i5-8250U: 400MHz - 3400MHz) */
    if (mhz < 400)  mhz = 400;
    if (mhz > 3400) mhz = 3400;

    /* Set via MSR (IA32_PERF_CTL) */
    uint64_t ratio = mhz / 100;
    __asm__ volatile(
        "wrmsr"
        :: "c"(0x199UL),                        /* IA32_PERF_CTL */
           "a"((uint32_t)(ratio << 8)),
           "d"(0UL)
    );

    record_result("CPU_FREQ_SET: mhz=%lld ok", mhz);
    serial_printf("[act] cpu_freq: %lld MHz\n", mhz);
}

/* <ACT:SLEEP_MS ms=N> */
static void
act_sleep_ms(const char *token) {
    int64_t ms = 0;
    parse_param_int(token, "ms", &ms);
    /* Simple busy-wait (PIT-based sleep in real impl) */
    for (int64_t i = 0; i < ms * 10000; i++) {
        __asm__ volatile("nop");
    }
    record_result("SLEEP_MS: %lld ms elapsed", ms);
}

/* <ACT:HARDWARE_QUERY> — returns full hardware status to AION */
static void
act_hardware_query(const char *token) {
    (void)token;
    MemoryStatus mem = memory_get_status();
    record_result(
        "HARDWARE: cpu=i5-8250U cores=4 threads=8 "
        "ram_total=%lluMB ram_free=%lluMB "
        "swap_total=%lluGB "
        "display=%dx%d "
        "cam=available mic=available audio=available "
        "usb=128GB network=available bluetooth=available",
        mem.total_physical / (1024*1024),
        mem.free_physical  / (1024*1024),
        mem.swap_total     / (1024ULL*1024*1024),
        display_get_width(), display_get_height()
    );
}

/* <ACT:DISPLAY_CLEAR> */
static void
act_display_clear(const char *token) {
    (void)token;
    display_clear();
    record_result("DISPLAY_CLEAR: ok");
}

/* ── Main dispatch ─────────────────────────────────────────────── */

typedef struct {
    const char *name;
    void (*handler)(const char *token);
} ActionDef;

static const ActionDef actions[] = {
    { "ACT:DISPLAY",         act_display         },
    { "ACT:DISPLAY_PIXEL",   act_display_pixel   },
    { "ACT:DISPLAY_CLEAR",   act_display_clear   },
    { "ACT:AUDIO_FREQ",      act_audio_freq      },
    { "ACT:AUDIO_PCM",       act_audio_pcm       },
    { "ACT:AUDIO_STOP",      act_audio_stop      },
    { "ACT:MEMORY_STORE",    act_memory_store    },
    { "ACT:MEMORY_RECALL",   act_memory_recall   },
    { "ACT:FILE_READ",       act_file_read       },
    { "ACT:FILE_WRITE",      act_file_write      },
    { "ACT:NET_FETCH",       act_net_fetch       },
    { "ACT:CPU_FREQ_SET",    act_cpu_freq_set    },
    { "ACT:SLEEP_MS",        act_sleep_ms        },
    { "ACT:HARDWARE_QUERY",  act_hardware_query  },
    { NULL, NULL }
};

void
action_parse_and_execute(const char *output) {
    if (!output) return;

    const char *p = output;
    int actions_executed = 0;

    while ((p = strchr(p, '<')) != NULL) {
        /* Try each known action */
        bool matched = false;
        for (int i = 0; actions[i].name != NULL; i++) {
            int nlen = strlen(actions[i].name);
            if (strncmp(p + 1, actions[i].name, nlen) == 0) {
                /* Find closing '>' */
                const char *end = strchr(p, '>');
                if (!end) break;

                /* Extract token (between < and >) */
                static char token_buf[2048];
                int tlen = end - p - 1;
                if (tlen >= 2048) tlen = 2047;
                memcpy(token_buf, p + 1, tlen);
                token_buf[tlen] = '\0';

                serial_printf("[act] executing: %s\n", actions[i].name);
                actions[i].handler(token_buf);
                actions_executed++;
                matched = true;
                p = end + 1;
                break;
            }
        }
        if (!matched) p++;
    }

    if (actions_executed > 0) {
        serial_printf("[act] %d actions executed this tick\n", actions_executed);
    }
}
