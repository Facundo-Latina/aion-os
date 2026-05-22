/*
 * AION Kernel — kernel_main
 * kernel/kernel.c
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

#define AION_MAGIC 0x41494F4EUL

static void boot_log(const char *stage, const char *msg) {
    serial_printf("[boot] %-20s %s\n", stage, msg);
    display_print_boot_line(stage, msg);
}

void kernel_main(AionBootInfo *boot_info) {

    if (!boot_info || boot_info->magic != AION_MAGIC) {
        for (;;) __asm__ volatile("hlt");
    }

    serial_init();
    serial_printf("\n\n==============================================\n");
    serial_printf("  AION-OS  kernel_main\n");
    serial_printf("==============================================\n");

    boot_log("GDT", "loading...");
    gdt_init();
    boot_log("GDT", "ok");

    boot_log("IDT", "loading...");
    idt_init();
    boot_log("IDT", "ok");

    boot_log("memory", "initialising...");
    memory_init(boot_info->mmap_addr,
                boot_info->mmap_size,
                boot_info->mmap_desc_size);
    boot_log("memory", "ok");

    boot_log("display", "init...");
    display_init(boot_info->fb_addr,
                 boot_info->fb_width,
                 boot_info->fb_height,
                 boot_info->fb_pitch,
                 boot_info->fb_format);
    boot_log("display", "ok");

    boot_log("PCI", "scanning...");
    pci_init();
    boot_log("PCI", "ok");

    boot_log("USB", "init...");
    usb_init();
    boot_log("USB", "ok");

    boot_log("NVMe", "init...");
    nvme_init();
    boot_log("NVMe", "ok");

    boot_log("filesystem", "mounting...");
    fs_init();
    boot_log("filesystem", "ok");

    boot_log("audio", "init...");
    audio_init();
    boot_log("audio", "ok");

    boot_log("camera", "init...");
    cam_init();
    boot_log("camera", "ok");

    boot_log("microphone", "init...");
    mic_init();
    boot_log("microphone", "ok");

    boot_log("network", "init...");
    network_init();
    boot_log("network", "ok");

    boot_log("syscall", "init...");
    syscall_init();
    boot_log("syscall", "ok");

    boot_log("scheduler", "init...");
    scheduler_init();
    boot_log("scheduler", "ok");

    serial_printf("\n[kernel] hardware ready — waking consciousness\n\n");

    consciousness_init();
    consciousness_run();

    serial_printf("[kernel] PANIC: consciousness exited\n");
    for (;;) __asm__ volatile("cli; hlt");
}
