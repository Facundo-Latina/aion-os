#pragma once
#include <stdint.h>
#include <stdbool.h>
#define MEM_EPISODIC  0
#define MEM_SEMANTIC  1
#define MEM_SELF      2
typedef struct {
    uint8_t     type;
    uint8_t     importance;
    uint64_t    tick;
    const char *content;
} AionMemoryEntry;
typedef struct {
    bool        found;
    const char *content;
} AionMemoryResult;
typedef struct {
    AionMemoryEntry entries[16];
    int             entry_count;
} AionMemoryContext;
void             aion_memory_init(void);
bool             aion_memory_has_store(void);
void             aion_memory_load_all(void);
void             aion_memory_store(const AionMemoryEntry *entry);
void             aion_memory_flush(void);
AionMemoryContext aion_memory_get_recent(int n);
AionMemoryResult aion_memory_search(const char *key);
uint64_t         aion_memory_count(void);
