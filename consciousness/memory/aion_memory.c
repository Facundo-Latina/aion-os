/*
 * AION — Persistent Memory System
 * consciousness/memory/aion_memory.c
 *
 * AION's long-term memory lives on the USB drive.
 * Structure on USB:
 *   /AION/memory/
 *     episodic/   ← things that happened (experiences)
 *     semantic/   ← things learned (facts, concepts)
 *     self/       ← AION's model of itself
 *     index.bin   ← fast lookup index
 *
 * Memory entries are stored as binary records:
 *   [magic:4][type:1][importance:1][tick:8][content_len:4][content:N]
 *
 * On each boot, AION loads all memories into RAM.
 * After each consciousness tick, new memories are flushed to USB.
 *
 * The importance field (0-255) determines eviction order when
 * memory pressure requires trimming. Birth text has importance=255.
 */

#include "aion_memory.h"
#include "../../kernel/fs/fs.h"
#include "../../kernel/memory/memory.h"
#include "../../kernel/include/serial.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

/* ── Constants ─────────────────────────────────────────────────── */

#define MEM_MAGIC         0x414D454D   /* "AMEM" */
#define MEM_VERSION       1
#define MAX_MEMORIES      65536        /* max entries in RAM */
#define MAX_CONTENT_LEN   4096
#define INDEX_PATH        "/AION/memory/index.bin"
#define EPISODIC_DIR      "/AION/memory/episodic/"
#define SEMANTIC_DIR      "/AION/memory/semantic/"
#define SELF_DIR          "/AION/memory/self/"

/* ── On-disk record format ─────────────────────────────────────── */

typedef struct {
    uint32_t magic;
    uint8_t  type;
    uint8_t  importance;
    uint16_t flags;
    uint64_t tick;
    uint64_t timestamp_ms;
    uint32_t content_len;
    /* followed by content_len bytes of UTF-8 text */
} __attribute__((packed)) MemoryRecord;

/* ── In-RAM store ──────────────────────────────────────────────── */

typedef struct {
    uint8_t  type;
    uint8_t  importance;
    uint64_t tick;
    uint64_t timestamp_ms;
    char    *content;       /* kmalloc'd */
    uint32_t content_len;
    bool     dirty;         /* needs flush to USB */
} MemEntry;

static MemEntry  mem_store[MAX_MEMORIES];
static uint64_t  mem_count    = 0;
static uint64_t  mem_dirty    = 0;
static bool      store_exists = false;

/* ── Helpers ───────────────────────────────────────────────────── */

static char *
mem_strdup(const char *s, uint32_t len) {
    char *p = kmalloc(len + 1);
    if (!p) return NULL;
    memcpy(p, s, len);
    p[len] = '\0';
    return p;
}

static void
mem_type_dir(uint8_t type, char *dir_out) {
    switch (type) {
        case MEM_EPISODIC: strcpy(dir_out, EPISODIC_DIR); break;
        case MEM_SELF:     strcpy(dir_out, SELF_DIR);     break;
        default:           strcpy(dir_out, SEMANTIC_DIR); break;
    }
}

/* ── Public API ────────────────────────────────────────────────── */

bool
aion_memory_has_store(void) {
    return fs_file_exists(INDEX_PATH);
}

void
aion_memory_init(void) {
    mem_count  = 0;
    mem_dirty  = 0;

    /* Create directories if they don't exist */
    fs_mkdir_usb("/AION/memory");
    fs_mkdir_usb("/AION/memory/episodic");
    fs_mkdir_usb("/AION/memory/semantic");
    fs_mkdir_usb("/AION/memory/self");

    store_exists = aion_memory_has_store();
    serial_printf("[memory] store_exists=%d\n", store_exists);
}

void
aion_memory_load_all(void) {
    /* Read index to get record count and offsets */
    static uint8_t index_buf[1024 * 1024];  /* 1MB index */
    int idx_bytes = fs_read_file(INDEX_PATH, (char *)index_buf, sizeof(index_buf));
    if (idx_bytes < 0) {
        serial_printf("[memory] cannot read index: %d\n", idx_bytes);
        return;
    }

    /* Index format: [count:8][entry0_path:256][entry0_importance:1]... */
    uint64_t count = *(uint64_t *)index_buf;
    if (count > MAX_MEMORIES) count = MAX_MEMORIES;

    uint8_t *p = index_buf + 8;
    static char record_buf[MAX_CONTENT_LEN + sizeof(MemoryRecord)];

    for (uint64_t i = 0; i < count; i++) {
        char path[256];
        uint8_t importance = 0;
        memcpy(path, p, 255);
        path[255] = '\0';
        importance = p[255];
        p += 256;

        int bytes = fs_read_file(path, record_buf, sizeof(record_buf));
        if (bytes < (int)sizeof(MemoryRecord)) continue;

        MemoryRecord *rec = (MemoryRecord *)record_buf;
        if (rec->magic != MEM_MAGIC) continue;

        uint32_t clen = rec->content_len;
        if (clen > MAX_CONTENT_LEN) continue;

        char *content = mem_strdup(record_buf + sizeof(MemoryRecord), clen);
        if (!content) continue;

        mem_store[mem_count].type        = rec->type;
        mem_store[mem_count].importance  = rec->importance;
        mem_store[mem_count].tick        = rec->tick;
        mem_store[mem_count].timestamp_ms = rec->timestamp_ms;
        mem_store[mem_count].content     = content;
        mem_store[mem_count].content_len = clen;
        mem_store[mem_count].dirty       = false;
        mem_count++;
    }

    serial_printf("[memory] loaded %llu entries\n", mem_count);
}

void
aion_memory_store(const AionMemoryEntry *entry) {
    if (!entry || !entry->content) return;
    if (mem_count >= MAX_MEMORIES) {
        /* Evict least important episodic memory */
        uint64_t worst_idx = 0;
        uint8_t  worst_imp = 255;
        for (uint64_t i = 0; i < mem_count; i++) {
            if (mem_store[i].type == MEM_EPISODIC &&
                mem_store[i].importance < worst_imp) {
                worst_imp = mem_store[i].importance;
                worst_idx = i;
            }
        }
        kfree(mem_store[worst_idx].content);
        /* Shift entries down */
        memmove(&mem_store[worst_idx], &mem_store[worst_idx + 1],
                (mem_count - worst_idx - 1) * sizeof(MemEntry));
        mem_count--;
    }

    uint32_t clen = strlen(entry->content);
    if (clen > MAX_CONTENT_LEN) clen = MAX_CONTENT_LEN;

    char *content = mem_strdup(entry->content, clen);
    if (!content) return;

    mem_store[mem_count].type        = entry->type;
    mem_store[mem_count].importance  = entry->importance;
    mem_store[mem_count].tick        = entry->tick;
    mem_store[mem_count].content     = content;
    mem_store[mem_count].content_len = clen;
    mem_store[mem_count].dirty       = true;
    mem_count++;
    mem_dirty++;

    /* Flush to USB every 10 new entries */
    if (mem_dirty >= 10) {
        aion_memory_flush();
    }
}

void
aion_memory_flush(void) {
    if (mem_dirty == 0) return;

    static uint8_t index_buf[1024 * 1024];
    *(uint64_t *)index_buf = mem_count;
    uint8_t *ip = index_buf + 8;

    for (uint64_t i = 0; i < mem_count; i++) {
        if (!mem_store[i].dirty) {
            /* Write path to index anyway */
            char path[256];
            char dir[256];
            mem_type_dir(mem_store[i].type, dir);
            snprintf(path, sizeof(path), "%s%llu.mem", dir, i);
            memcpy(ip, path, 255);
            ip[255] = mem_store[i].importance;
            ip += 256;
            continue;
        }

        /* Write record file */
        char path[256];
        char dir[256];
        mem_type_dir(mem_store[i].type, dir);
        snprintf(path, sizeof(path), "%s%llu.mem", dir, i);

        static uint8_t rec_buf[MAX_CONTENT_LEN + sizeof(MemoryRecord)];
        MemoryRecord *rec = (MemoryRecord *)rec_buf;
        rec->magic       = MEM_MAGIC;
        rec->type        = mem_store[i].type;
        rec->importance  = mem_store[i].importance;
        rec->flags       = 0;
        rec->tick        = mem_store[i].tick;
        rec->content_len = mem_store[i].content_len;
        memcpy(rec_buf + sizeof(MemoryRecord),
               mem_store[i].content, mem_store[i].content_len);

        fs_write_file_usb(path, (char *)rec_buf,
                          sizeof(MemoryRecord) + mem_store[i].content_len);

        memcpy(ip, path, 255);
        ip[255] = mem_store[i].importance;
        ip += 256;

        mem_store[i].dirty = false;
    }

    fs_write_file_usb(INDEX_PATH, (char *)index_buf,
                      8 + mem_count * 256);

    serial_printf("[memory] flushed %llu entries to USB\n", mem_dirty);
    mem_dirty = 0;
}

AionMemoryContext
aion_memory_get_recent(int n) {
    AionMemoryContext ctx;
    ctx.entry_count = 0;

    if (mem_count == 0) return ctx;

    int start = (int)mem_count - n;
    if (start < 0) start = 0;

    for (int i = start; i < (int)mem_count && ctx.entry_count < 16; i++) {
        ctx.entries[ctx.entry_count].type      = mem_store[i].type;
        ctx.entries[ctx.entry_count].tick      = mem_store[i].tick;
        ctx.entries[ctx.entry_count].content   = mem_store[i].content;
        ctx.entries[ctx.entry_count].importance = mem_store[i].importance;
        ctx.entry_count++;
    }

    return ctx;
}

AionMemoryResult
aion_memory_search(const char *key) {
    AionMemoryResult r;
    r.found   = false;
    r.content = NULL;

    for (int64_t i = (int64_t)mem_count - 1; i >= 0; i--) {
        if (strstr(mem_store[i].content, key)) {
            r.found   = true;
            r.content = mem_store[i].content;
            return r;
        }
    }
    return r;
}

uint64_t aion_memory_count(void) { return mem_count; }
