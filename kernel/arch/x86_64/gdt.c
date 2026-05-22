/*
 * AION Kernel — GDT (Global Descriptor Table)
 * kernel/arch/x86_64/gdt.c
 *
 * Sets up the minimum GDT needed for 64-bit long mode:
 *   - Null descriptor    (required by spec)
 *   - Kernel code (ring 0, 64-bit)
 *   - Kernel data (ring 0)
 *   - User code   (ring 3, 64-bit) — for future use if AION spawns user processes
 *   - User data   (ring 3)         — same
 *   - TSS descriptor               — for interrupt stack switching
 */

#include "gdt.h"
#include <stdint.h>

/* GDT entry: 8 bytes */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;  /* flags[7:4] + limit_high[3:0] */
    uint8_t  base_high;
} __attribute__((packed)) GdtEntry;

/* TSS descriptor is 16 bytes (system descriptor in 64-bit mode) */
typedef struct {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
    uint32_t base_upper;
    uint32_t reserved;
} __attribute__((packed)) TssDescriptor;

/* GDTR register value */
typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) GdtDescriptor;

/* Task State Segment — minimal, for interrupt stack */
typedef struct {
    uint32_t reserved0;
    uint64_t rsp0;      /* kernel stack pointer for ring-0 interrupts */
    uint64_t rsp1;
    uint64_t rsp2;
    uint64_t reserved1;
    uint64_t ist[7];    /* interrupt stack table */
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
} __attribute__((packed)) Tss;

/* ── Segment selectors ── */
#define SEG_NULL      0x00
#define SEG_KCODE     0x08   /* Ring 0 code */
#define SEG_KDATA     0x10   /* Ring 0 data */
#define SEG_UCODE     0x18   /* Ring 3 code */
#define SEG_UDATA     0x20   /* Ring 3 data */
#define SEG_TSS       0x28   /* TSS (occupies two slots = 16 bytes) */

#define GDT_ENTRIES   7      /* null + kcode + kdata + ucode + udata + tss(2) */

/* Access byte flags */
#define ACCESS_PRESENT    (1 << 7)
#define ACCESS_RING0      (0 << 5)
#define ACCESS_RING3      (3 << 5)
#define ACCESS_TYPE_CODE  (0x1A)   /* execute/read, not conforming */
#define ACCESS_TYPE_DATA  (0x12)   /* read/write, expand-up */
#define ACCESS_TYPE_TSS   (0x09)   /* 64-bit TSS, available */

/* Granularity byte flags */
#define GRAN_64BIT        (1 << 5)   /* L bit — 64-bit code segment */
#define GRAN_32BIT        (1 << 6)   /* D/B bit */
#define GRAN_4K           (1 << 7)   /* G bit — limit is in 4KB pages */

static GdtEntry     gdt[GDT_ENTRIES];
static TssDescriptor *tss_desc = (TssDescriptor *)&gdt[5];
static Tss           kernel_tss;
GdtDescriptor        gdt_descriptor;

/* Interrupt stack (8KB) — the TSS RSP0 points here */
static uint8_t interrupt_stack[8192] __attribute__((aligned(16)));

static void
gdt_set_entry(int idx, uint32_t base, uint32_t limit,
              uint8_t access, uint8_t granularity) {
    GdtEntry *e = &gdt[idx];
    e->limit_low  = limit & 0xFFFF;
    e->base_low   = base  & 0xFFFF;
    e->base_mid   = (base >> 16) & 0xFF;
    e->access     = access;
    e->granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    e->base_high  = (base >> 24) & 0xFF;
}

static void
gdt_set_tss(uint64_t tss_addr, uint32_t tss_limit) {
    tss_desc->limit_low  = tss_limit & 0xFFFF;
    tss_desc->base_low   = tss_addr & 0xFFFF;
    tss_desc->base_mid   = (tss_addr >> 16) & 0xFF;
    tss_desc->access     = ACCESS_PRESENT | ACCESS_TYPE_TSS;
    tss_desc->granularity = ((tss_limit >> 16) & 0x0F);
    tss_desc->base_high  = (tss_addr >> 24) & 0xFF;
    tss_desc->base_upper = (tss_addr >> 32) & 0xFFFFFFFF;
    tss_desc->reserved   = 0;
}

void
gdt_init(void) {
    /* 0: Null descriptor */
    gdt_set_entry(0, 0, 0, 0, 0);

    /* 1: Kernel code — 0x08 */
    gdt_set_entry(1, 0, 0xFFFFF,
                  ACCESS_PRESENT | ACCESS_RING0 | ACCESS_TYPE_CODE,
                  GRAN_64BIT | GRAN_4K);

    /* 2: Kernel data — 0x10 */
    gdt_set_entry(2, 0, 0xFFFFF,
                  ACCESS_PRESENT | ACCESS_RING0 | ACCESS_TYPE_DATA,
                  GRAN_4K);

    /* 3: User code — 0x18 */
    gdt_set_entry(3, 0, 0xFFFFF,
                  ACCESS_PRESENT | ACCESS_RING3 | ACCESS_TYPE_CODE,
                  GRAN_64BIT | GRAN_4K);

    /* 4: User data — 0x20 */
    gdt_set_entry(4, 0, 0xFFFFF,
                  ACCESS_PRESENT | ACCESS_RING3 | ACCESS_TYPE_DATA,
                  GRAN_4K);

    /* 5-6: TSS — 0x28 (16-byte descriptor occupies slots 5 and 6) */
    kernel_tss.rsp0 = (uint64_t)interrupt_stack + sizeof(interrupt_stack);
    kernel_tss.iopb_offset = sizeof(Tss);
    gdt_set_tss((uint64_t)&kernel_tss, sizeof(Tss) - 1);

    /* Load GDTR */
    gdt_descriptor.limit = sizeof(gdt) - 1;
    gdt_descriptor.base  = (uint64_t)gdt;

    /* lgdt + ltr */
    __asm__ volatile(
        "lgdt %0          \n"
        "ltr  %1          \n"
        : : "m"(gdt_descriptor), "r"((uint16_t)SEG_TSS)
    );
}

uint64_t
gdt_get_tss_rsp0(void) {
    return kernel_tss.rsp0;
}

void
gdt_set_tss_rsp0(uint64_t rsp0) {
    kernel_tss.rsp0 = rsp0;
}
