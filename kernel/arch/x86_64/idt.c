/*
 * AION Kernel — IDT (Interrupt Descriptor Table)
 * kernel/arch/x86_64/idt.c
 *
 * Sets up hardware interrupt and exception handling.
 * Also initialises the 8259A PIC (legacy, supported on all x86 hardware).
 *
 * AION uses interrupts for:
 *   - Hardware events (keyboard, timer, USB, audio DMA, camera DMA)
 *   - CPU exceptions (page fault = swap needed, etc.)
 *
 * All unhandled interrupts route to a generic handler that logs and returns.
 * The consciousness runtime registers its own callbacks via idt_register_handler().
 */

#include "idt.h"
#include "../include/io.h"
#include <stdint.h>
#include <stddef.h>

/* IDT entry: 16 bytes in 64-bit mode */
typedef struct {
    uint16_t offset_low;
    uint16_t selector;     /* code segment — always 0x08 (kernel code) */
    uint8_t  ist;          /* interrupt stack table index (0 = none) */
    uint8_t  type_attr;    /* type + DPL + present bit */
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
} __attribute__((packed)) IdtEntry;

typedef struct {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed)) IdtDescriptor;

#define IDT_ENTRIES 256
#define IDT_PRESENT (1 << 7)
#define IDT_RING0   (0 << 5)
#define IDT_INT_GATE  0x0E   /* interrupt gate (disables interrupts on entry) */
#define IDT_TRAP_GATE 0x0F   /* trap gate (interrupts stay enabled) */

static IdtEntry    idt[IDT_ENTRIES];
static IdtDescriptor idt_descriptor;

/* User-registered handlers — AION consciousness registers here */
static void (*user_handlers[IDT_ENTRIES])(uint8_t irq, void *ctx);
static void *user_handler_ctx[IDT_ENTRIES];

/* ── PIC (8259A) ─────────────────────────────────────────────────── */
/* We remap IRQs 0-15 to vectors 0x20-0x2F to avoid collision with
   CPU exceptions (0x00-0x1F) */

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1
#define PIC_EOI   0x20

#define IRQ_BASE  0x20   /* hardware IRQs start at vector 0x20 */

static void
pic_remap(void) {
    /* Save masks */
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    /* ICW1: init + ICW4 needed */
    outb(PIC1_CMD,  0x11);
    outb(PIC2_CMD,  0x11);
    io_wait();

    /* ICW2: vector offsets */
    outb(PIC1_DATA, IRQ_BASE);       /* IRQ 0-7  -> 0x20-0x27 */
    outb(PIC2_DATA, IRQ_BASE + 8);   /* IRQ 8-15 -> 0x28-0x2F */
    io_wait();

    /* ICW3: cascade */
    outb(PIC1_DATA, 0x04);  /* slave on IRQ2 */
    outb(PIC2_DATA, 0x02);  /* slave ID = 2 */
    io_wait();

    /* ICW4: 8086 mode */
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    io_wait();

    /* Restore masks */
    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

static void
pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, PIC_EOI);
    outb(PIC1_CMD, PIC_EOI);
}

static void
pic_mask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) | (1 << irq));
}

static void
pic_unmask(uint8_t irq) {
    uint16_t port = (irq < 8) ? PIC1_DATA : PIC2_DATA;
    if (irq >= 8) irq -= 8;
    outb(port, inb(port) & ~(1 << irq));
}

/* ── IDT entry setup ─────────────────────────────────────────────── */

static void
idt_set_entry(int vec, uint64_t handler, uint8_t type_attr, uint8_t ist) {
    IdtEntry *e = &idt[vec];
    e->offset_low  = handler & 0xFFFF;
    e->selector    = 0x08;
    e->ist         = ist;
    e->type_attr   = IDT_PRESENT | IDT_RING0 | type_attr;
    e->offset_mid  = (handler >> 16) & 0xFFFF;
    e->offset_high = (handler >> 32) & 0xFFFFFFFF;
    e->reserved    = 0;
}

/* ── Generic handler (C side) ────────────────────────────────────── */

/*
 * All interrupt stubs (defined in idt_stubs.asm) call this function.
 * 'vec' is the interrupt vector number.
 */
void
idt_dispatch(uint8_t vec, uint64_t error_code) {
    (void)error_code;

    /* Call user-registered handler if present */
    if (user_handlers[vec] != NULL) {
        user_handlers[vec](vec, user_handler_ctx[vec]);
    }

    /* Send EOI for hardware IRQs (0x20-0x2F) */
    if (vec >= IRQ_BASE && vec < IRQ_BASE + 16) {
        pic_send_eoi(vec - IRQ_BASE);
    }
}

/* ── Public API ──────────────────────────────────────────────────── */

void
idt_register_handler(uint8_t vec, void (*handler)(uint8_t, void *), void *ctx) {
    user_handlers[vec] = handler;
    user_handler_ctx[vec] = ctx;
    /* Unmask PIC line for hardware IRQs */
    if (vec >= IRQ_BASE && vec < IRQ_BASE + 16) {
        pic_unmask(vec - IRQ_BASE);
    }
}

void
idt_unregister_handler(uint8_t vec) {
    user_handlers[vec] = NULL;
    user_handler_ctx[vec] = NULL;
    if (vec >= IRQ_BASE && vec < IRQ_BASE + 16) {
        pic_mask(vec - IRQ_BASE);
    }
}

/* Defined in idt_stubs.asm — one stub per vector */
extern void isr_stub_0(void);
extern void isr_stub_1(void);
extern void isr_stub_2(void);
extern void isr_stub_3(void);
extern void isr_stub_4(void);
extern void isr_stub_5(void);
extern void isr_stub_6(void);
extern void isr_stub_7(void);
extern void isr_stub_8(void);
extern void isr_stub_13(void);
extern void isr_stub_14(void);
/* IRQ hardware stubs: 0x20-0x2F */
extern void irq_stub_0(void);   /* PIT timer */
extern void irq_stub_1(void);   /* Keyboard */
extern void irq_stub_2(void);   /* Cascade */
extern void irq_stub_3(void);   /* COM2 */
extern void irq_stub_4(void);   /* COM1 */
extern void irq_stub_8(void);   /* RTC */
extern void irq_stub_9(void);   /* General */
extern void irq_stub_11(void);  /* Network / USB */
extern void irq_stub_12(void);  /* PS/2 mouse */
extern void irq_stub_14(void);  /* Primary ATA */
extern void irq_stub_15(void);  /* Secondary ATA */
/* Generic fallback for unregistered vectors */
extern void irq_stub_generic(void);

void
idt_init(void) {
    /* Install stubs for CPU exceptions */
    idt_set_entry(0,  (uint64_t)isr_stub_0,  IDT_INT_GATE, 0);
    idt_set_entry(1,  (uint64_t)isr_stub_1,  IDT_INT_GATE, 0);
    idt_set_entry(2,  (uint64_t)isr_stub_2,  IDT_INT_GATE, 0);
    idt_set_entry(3,  (uint64_t)isr_stub_3,  IDT_TRAP_GATE, 0);
    idt_set_entry(4,  (uint64_t)isr_stub_4,  IDT_INT_GATE, 0);
    idt_set_entry(5,  (uint64_t)isr_stub_5,  IDT_INT_GATE, 0);
    idt_set_entry(6,  (uint64_t)isr_stub_6,  IDT_INT_GATE, 0);
    idt_set_entry(7,  (uint64_t)isr_stub_7,  IDT_INT_GATE, 0);
    idt_set_entry(8,  (uint64_t)isr_stub_8,  IDT_INT_GATE, 1); /* IST1 for DF */
    idt_set_entry(13, (uint64_t)isr_stub_13, IDT_INT_GATE, 0);
    idt_set_entry(14, (uint64_t)isr_stub_14, IDT_INT_GATE, 0);

    /* Hardware IRQs — map PIC lines to vectors */
    idt_set_entry(0x20, (uint64_t)irq_stub_0,  IDT_INT_GATE, 0);
    idt_set_entry(0x21, (uint64_t)irq_stub_1,  IDT_INT_GATE, 0);
    idt_set_entry(0x22, (uint64_t)irq_stub_2,  IDT_INT_GATE, 0);
    idt_set_entry(0x23, (uint64_t)irq_stub_3,  IDT_INT_GATE, 0);
    idt_set_entry(0x24, (uint64_t)irq_stub_4,  IDT_INT_GATE, 0);
    idt_set_entry(0x28, (uint64_t)irq_stub_8,  IDT_INT_GATE, 0);
    idt_set_entry(0x29, (uint64_t)irq_stub_9,  IDT_INT_GATE, 0);
    idt_set_entry(0x2B, (uint64_t)irq_stub_11, IDT_INT_GATE, 0);
    idt_set_entry(0x2C, (uint64_t)irq_stub_12, IDT_INT_GATE, 0);
    idt_set_entry(0x2E, (uint64_t)irq_stub_14, IDT_INT_GATE, 0);
    idt_set_entry(0x2F, (uint64_t)irq_stub_15, IDT_INT_GATE, 0);

    /* Remap PIC before loading IDT */
    pic_remap();

    /* Mask all IRQs by default — drivers unmask what they need */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);

    /* Load IDTR */
    idt_descriptor.limit = sizeof(idt) - 1;
    idt_descriptor.base  = (uint64_t)idt;
    __asm__ volatile("lidt %0" : : "m"(idt_descriptor));

    /* Enable interrupts */
    __asm__ volatile("sti");
}
