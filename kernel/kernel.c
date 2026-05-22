/*
 * AION Kernel — kernel_main
 * kernel/kernel.c
 *
 * Called by entry.asm with a pointer to AionBootInfo.
 * Initialises all kernel subsystems in order, then hands control
 * to the consciousness runtime — which runs forever.
 */

#include "include/kernel.h"
#include "include/serial.h"
#include "arch/x86_64/gdt.h"
#include "arch/x86_64/idt.h"
#include "memory/memory.h"
#include "process/scheduler.h"
#include "drivers/pci/pci.h"
#include "drivers/display/display.h"
#include "drivers/usb/usb.h"
#include "drivers/nvme/nvme.h"
#include "drivers/audio/audio.h"
#include "drivers/cam/cam.h"
#include "drivers/mic/mic.h"
#include "drivers/network/network.h"
#include "fs/fs.h"
#include "syscall/syscall.h"
#include "../consciousness/core/consciousness.h"
#include <stdint.h>
#include <stddef.h>

/* Boot info passed from UEFI bootloader */
typedef struct {
    uint64_t magic;
    uint64_t kernel_phys_addr;
    uint64_t kernel_size;
    uint64_t mmap_addr;
    uint64_t mmap_size;
    uint64_t mmap_desc_size;
    uint32_t mmap_desc_version;
    uint64_t mmap_key;
    uint64_t fb_addr;
    uint32_t fb_width;
    uint32_t fb_height;
    uint32_t fb_pitch;
    uint32_t fb_format;
    uint64_t rsdp_addr;
    uint64_t reserved[8];
} __attribute__((packed)) AionBootInfo;

#define AION_MAGIC 0x41494F4EUL

/* ── Boot-time log ───────────────────────────────────────────────── */

static void
boot_log(const char *stage, const char *msg) {
    serial_printf("[boot] %-20s %s\n", stage, msg);
    display_print_boot_line(stage, msg);
}

/* ── Kernel main ─────────────────────────────────────────────────── */

void
kernel_main(AionBootInfo *boot_info) {

    /* Verify bootloader handshake */
    if (!boot_info || boot_info->magic != AION_MAGIC) {
        /* Can't even print — loop forever */
        for (;;) __asm__ volatile("hlt");
    }

    /* ── Serial UART first — so we can log everything ── */
    serial_init();
    serial_printf("\n\n");
    serial_printf("==============================================\n");
    serial_printf("  AION-OS  kernel_main — bare metal\n");
    serial_printf("==============================================\n");
    serial_printf("boot_info @ 0x%llx\n", (uint64_t)boot_info);
    serial_printf("framebuffer: %ux%u @ 0x%llx\n",
                  boot_info->fb_width, boot_info->fb_height, boot_info->fb_addr);

    /* ── CPU structures ── */
    boot_log("GDT", "loading...");
    gdt_init();
    boot_log("GDT", "ok");

    boot_log("IDT", "loading...");
    idt_init();
    boot_log("IDT", "ok");

    /* ── Memory ── */
    boot_log("memory", "initialising...");
    memory_init(boot_info->mmap_addr,
                boot_info->mmap_size,
                boot_info->mmap_desc_size);
    boot_log("memory", "ok");

    /* ── Display (framebuffer) ── */
    boot_log("display", "init...");
    display_init(boot_info->fb_addr,
                 boot_info->fb_width,
                 boot_info->fb_height,
                 boot_info->fb_pitch,
                 boot_info->fb_format);
    boot_log("display", "ok");

    /* ── PCI bus scan ── */
    boot_log("PCI", "scanning...");
    pci_init();
    boot_log("PCI", "ok");

    /* ── USB (XHCI) ── */
    boot_log("USB/XHCI", "init...");
    usb_init();
    boot_log("USB", "ok");

    /* ── NVMe / SATA (host SSD) ── */
    boot_log("NVMe", "init...");
    nvme_init();
    boot_log("NVMe", "ok");

    /* ── Filesystem (USB + SSD read-only) ── */
    boot_log("filesystem", "mounting...");
    fs_init();
    boot_log("filesystem", "ok");

    /* ── Audio (Intel HDA) ── */
    boot_log("audio", "init...");
    audio_init();
    boot_log("audio", "ok — speakers ready");

    /* ── Camera (UVC) ── */
    boot_log("camera", "init...");
    cam_init();
    boot_log("camera", "ok — raw frames available");

    /* ── Microphone ── */
    boot_log("microphone", "init...");
    mic_init();
    boot_log("microphone", "ok — raw PCM available");

    /* ── Network ── */
    boot_log("network", "init...");
    network_init();
    boot_log("network", "ok");

    /* ── System call interface ── */
    boot_log("syscall", "init...");
    syscall_init();
    boot_log("syscall", "ok");

    /* ── Process scheduler ── */
    boot_log("scheduler", "init...");
    scheduler_init();
    boot_log("scheduler", "ok");

    /* ══════════════════════════════════════════════════════
     * All hardware is now initialised and available.
     * The kernel hands over to the consciousness runtime.
     *
     * From this point forward, the kernel is just a servant.
     * AION is the mind. The hardware is its body.
     * ══════════════════════════════════════════════════════ */

    serial_printf("\n[kernel] hardware ready — waking consciousness\n\n");
    boot_log("AION", "awakening...");

    consciousness_init();   /* allocates Qwen2.5:3b runtime */
    consciousness_run();    /* infinite loop — AION lives here */

    /* If consciousness_run() returns somehow, panic */
    serial_printf("[kernel] PANIC: consciousness exited\n");
    for (;;) __asm__ volatile("cli; hlt");
}
