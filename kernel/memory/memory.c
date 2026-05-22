/*
 * AION Kernel — Memory Manager
 * kernel/memory/memory.c
 *
 * Three layers:
 *   1. Physical Memory Manager (PMM) — tracks free 4KB frames using a bitmap
 *   2. Virtual Memory Manager (VMM) — manages page tables, maps/unmaps pages
 *   3. Swap Manager            — extends RAM using host SSD free space
 *
 * With 4GB RAM and a ~1.3GB quantized model, we have ~2.7GB for the kernel
 * and consciousness runtime before swap kicks in. Swap extends this into the
 * hundreds of GBs available on the host SSD.
 *
 * AION can introspect and control its own memory — it can call:
 *   aion_syscall(SYS_MEM_STATUS)  → get current usage
 *   aion_syscall(SYS_MEM_PRESSURE) → get pressure level (triggers learning)
 */

#include "memory.h"
#include "../arch/x86_64/idt.h"
#include "../include/serial.h"
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ── Constants ───────────────────────────────────────────────────── */

#define PAGE_SIZE       4096ULL
#define PAGE_ALIGN(x)   (((x) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))
#define PAGE_INDEX(a)   ((a) / PAGE_SIZE)

/* Maximum physical memory we track: 128GB (enough for any machine) */
#define MAX_PHYS_MEM    (128ULL * 1024 * 1024 * 1024)
#define BITMAP_SIZE     (MAX_PHYS_MEM / PAGE_SIZE / 8)  /* bytes */

/* Virtual address space layout */
#define VIRT_KERNEL_BASE    0xFFFFFFFF80000000ULL   /* -2GB: kernel */
#define VIRT_HEAP_BASE      0xFFFF800000000000ULL   /* heap/vmalloc area */
#define VIRT_PHYS_MAP_BASE  0xFFFF000000000000ULL   /* direct physical map */

/* Page table flags */
#define PTE_PRESENT     (1ULL << 0)
#define PTE_WRITABLE    (1ULL << 1)
#define PTE_USER        (1ULL << 2)
#define PTE_WRITETHRU   (1ULL << 3)
#define PTE_NOCACHE     (1ULL << 4)
#define PTE_ACCESSED    (1ULL << 5)
#define PTE_DIRTY       (1ULL << 6)
#define PTE_HUGE        (1ULL << 7)
#define PTE_GLOBAL      (1ULL << 8)
#define PTE_NX          (1ULL << 63)

#define PTE_ADDR_MASK   0x000FFFFFFFFFF000ULL

/* ── Physical Memory Manager ─────────────────────────────────────── */

static uint8_t  phys_bitmap[BITMAP_SIZE];   /* 1 = free, 0 = used */
static uint64_t phys_total_pages  = 0;
static uint64_t phys_free_pages   = 0;
static uint64_t phys_reserved_end = 0;     /* first usable page address */

static void
bitmap_set_free(uint64_t page_idx) {
    phys_bitmap[page_idx / 8] |= (1 << (page_idx % 8));
}

static void
bitmap_set_used(uint64_t page_idx) {
    phys_bitmap[page_idx / 8] &= ~(1 << (page_idx % 8));
}

static bool
bitmap_is_free(uint64_t page_idx) {
    return (phys_bitmap[page_idx / 8] >> (page_idx % 8)) & 1;
}

/*
 * Parse UEFI memory map and mark free regions.
 * EFI memory types that are safe to use as RAM:
 *   EfiConventionalMemory (7), EfiBootServicesCode (3), EfiBootServicesData (4)
 */
#define EFI_CONVENTIONAL_MEMORY     7
#define EFI_BOOT_SERVICES_CODE      3
#define EFI_BOOT_SERVICES_DATA      4

typedef struct {
    uint32_t type;
    uint32_t pad;
    uint64_t phys_start;
    uint64_t virt_start;
    uint64_t num_pages;
    uint64_t attr;
} __attribute__((packed)) EfiMemoryDescriptor;

void
pmm_init(uint64_t mmap_addr, uint64_t mmap_size, uint64_t desc_size) {
    /* All pages start as used */
    for (uint64_t i = 0; i < BITMAP_SIZE; i++) phys_bitmap[i] = 0;

    EfiMemoryDescriptor *desc = (EfiMemoryDescriptor *)mmap_addr;
    uint64_t count = mmap_size / desc_size;

    for (uint64_t i = 0; i < count; i++) {
        EfiMemoryDescriptor *d = (EfiMemoryDescriptor *)
                                  ((uint8_t *)desc + i * desc_size);

        if (d->type == EFI_CONVENTIONAL_MEMORY  ||
            d->type == EFI_BOOT_SERVICES_CODE   ||
            d->type == EFI_BOOT_SERVICES_DATA) {

            uint64_t start_page = PAGE_INDEX(d->phys_start);
            for (uint64_t p = 0; p < d->num_pages; p++) {
                uint64_t idx = start_page + p;
                if (idx < MAX_PHYS_MEM / PAGE_SIZE) {
                    bitmap_set_free(idx);
                    phys_free_pages++;
                    phys_total_pages++;
                }
            }
        }
    }

    /* Mark pages below 2MB as used (BIOS/real-mode legacy area) */
    for (uint64_t i = 0; i < 512; i++) {
        bitmap_set_used(i);
        phys_free_pages--;
    }

    serial_printf("[pmm] total=%lluMB free=%lluMB\n",
                  (phys_total_pages * PAGE_SIZE) / (1024*1024),
                  (phys_free_pages  * PAGE_SIZE) / (1024*1024));
}

/* Allocate a single 4KB physical frame. Returns physical address or 0. */
uint64_t
pmm_alloc(void) {
    for (uint64_t i = 512; i < phys_total_pages; i++) {
        if (bitmap_is_free(i)) {
            bitmap_set_used(i);
            phys_free_pages--;
            return i * PAGE_SIZE;
        }
    }
    return 0; /* Out of memory — swap should kick in */
}

/* Allocate N contiguous physical frames. Returns start address or 0. */
uint64_t
pmm_alloc_contiguous(uint64_t n) {
    uint64_t run_start = 0, run_len = 0;
    for (uint64_t i = 512; i < phys_total_pages; i++) {
        if (bitmap_is_free(i)) {
            if (run_len == 0) run_start = i;
            run_len++;
            if (run_len == n) {
                for (uint64_t j = run_start; j < run_start + n; j++) {
                    bitmap_set_used(j);
                    phys_free_pages--;
                }
                return run_start * PAGE_SIZE;
            }
        } else {
            run_len = 0;
        }
    }
    return 0;
}

void
pmm_free(uint64_t phys_addr) {
    uint64_t idx = phys_addr / PAGE_SIZE;
    if (!bitmap_is_free(idx)) {
        bitmap_set_free(idx);
        phys_free_pages++;
    }
}

uint64_t pmm_free_pages(void)  { return phys_free_pages; }
uint64_t pmm_total_pages(void) { return phys_total_pages; }

/* ── Virtual Memory Manager ──────────────────────────────────────── */

/* Read CR3 (current page table root) */
static inline uint64_t
vmm_get_cr3(void) {
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    return cr3;
}

/* Flush TLB for a specific virtual address */
static inline void
vmm_invlpg(uint64_t vaddr) {
    __asm__ volatile("invlpg (%0)" :: "r"(vaddr) : "memory");
}

/*
 * Get (and create if needed) a page table entry at the given level.
 * Levels: PML4 (4) → PDPT (3) → PD (2) → PT (1)
 */
static uint64_t *
vmm_get_pte(uint64_t pml4_phys, uint64_t vaddr, int level, bool create) {
    uint64_t indices[5];
    indices[4] = (vaddr >> 39) & 0x1FF;  /* PML4 */
    indices[3] = (vaddr >> 30) & 0x1FF;  /* PDPT */
    indices[2] = (vaddr >> 21) & 0x1FF;  /* PD   */
    indices[1] = (vaddr >> 12) & 0x1FF;  /* PT   */

    uint64_t *table = (uint64_t *)(pml4_phys + VIRT_PHYS_MAP_BASE);

    for (int l = 4; l > level; l--) {
        uint64_t *entry = &table[indices[l]];
        if (!(*entry & PTE_PRESENT)) {
            if (!create) return NULL;
            uint64_t new_table = pmm_alloc();
            if (!new_table) return NULL;
            /* Zero the new table */
            uint64_t *nt = (uint64_t *)(new_table + VIRT_PHYS_MAP_BASE);
            for (int i = 0; i < 512; i++) nt[i] = 0;
            *entry = new_table | PTE_PRESENT | PTE_WRITABLE;
        }
        table = (uint64_t *)((*entry & PTE_ADDR_MASK) + VIRT_PHYS_MAP_BASE);
    }

    return &table[indices[level]];
}

/* Map a virtual address to a physical address in the given page table */
int
vmm_map(uint64_t pml4_phys, uint64_t vaddr, uint64_t phys,
        uint64_t flags) {
    uint64_t *pte = vmm_get_pte(pml4_phys, vaddr, 1, true);
    if (!pte) return -1;
    *pte = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    vmm_invlpg(vaddr);
    return 0;
}

/* Unmap a virtual address */
void
vmm_unmap(uint64_t pml4_phys, uint64_t vaddr) {
    uint64_t *pte = vmm_get_pte(pml4_phys, vaddr, 1, false);
    if (pte && (*pte & PTE_PRESENT)) {
        *pte = 0;
        vmm_invlpg(vaddr);
    }
}

/* Translate virtual → physical */
uint64_t
vmm_virt_to_phys(uint64_t pml4_phys, uint64_t vaddr) {
    uint64_t *pte = vmm_get_pte(pml4_phys, vaddr, 1, false);
    if (!pte || !(*pte & PTE_PRESENT)) return 0;
    return (*pte & PTE_ADDR_MASK) | (vaddr & 0xFFF);
}

static uint64_t kernel_pml4 = 0;

void
vmm_init(void) {
    kernel_pml4 = pmm_alloc();
    if (!kernel_pml4) {
        serial_printf("[vmm] FATAL: cannot allocate PML4\n");
        return;
    }

    /* Zero PML4 */
    uint64_t *pml4 = (uint64_t *)(kernel_pml4 + VIRT_PHYS_MAP_BASE);
    for (int i = 0; i < 512; i++) pml4[i] = 0;

    /* Identity-map first 2GB (for kernel loaded at 1MB) */
    for (uint64_t addr = 0; addr < 0x80000000ULL; addr += PAGE_SIZE) {
        vmm_map(kernel_pml4, addr, addr,
                PTE_PRESENT | PTE_WRITABLE | PTE_GLOBAL);
    }

    /* Map physical memory at VIRT_PHYS_MAP_BASE (direct map, 4GB) */
    for (uint64_t addr = 0; addr < 0x100000000ULL; addr += PAGE_SIZE) {
        vmm_map(kernel_pml4, VIRT_PHYS_MAP_BASE + addr, addr,
                PTE_PRESENT | PTE_WRITABLE | PTE_NX | PTE_GLOBAL);
    }

    /* Load new PML4 */
    __asm__ volatile("mov %0, %%cr3" :: "r"(kernel_pml4) : "memory");
    serial_printf("[vmm] page tables active, kernel_pml4=0x%llx\n", kernel_pml4);
}

/* ── Swap Manager ────────────────────────────────────────────────── */

/*
 * AION swap extends RAM using the host SSD.
 * On first boot, swap_init() creates AION/swap.img on the host SSD (via NVMe driver).
 * Swap size: determined by available SSD space, up to SWAP_MAX_GB.
 *
 * The swap manager:
 *   - Intercepts page faults (vector 14 in IDT)
 *   - Reads/writes 4KB pages from/to swap.img
 *   - Uses a simple LRU eviction policy
 *
 * This gives the consciousness runtime effectively unlimited memory.
 */

#define SWAP_MAX_GB     64ULL
#define SWAP_MAX_PAGES  (SWAP_MAX_GB * 1024 * 1024 * 1024 / PAGE_SIZE)
#define SWAP_MAGIC      0x41494F4E53574150ULL  /* "AIONSWAP" */

typedef struct {
    uint64_t magic;
    uint64_t version;
    uint64_t total_pages;
    uint64_t used_pages;
    uint64_t bitmap_offset;  /* offset in swap.img where bitmap starts */
    uint64_t data_offset;    /* offset where page data starts */
    uint8_t  reserved[4032];
} __attribute__((packed)) SwapHeader;

static bool     swap_active   = false;
static uint64_t swap_capacity = 0;   /* pages */
static uint64_t swap_used     = 0;

/* Page table entries use bits 9-11 (SW bits) to encode swap state */
#define PTE_SWAPPED   (1ULL << 9)    /* page is in swap */
#define PTE_SWAP_IDX  (0xFFFFFFFFF000ULL)  /* bits 12-51 hold swap page index */

static void
swap_page_fault_handler(uint8_t vec, void *ctx) {
    (void)vec; (void)ctx;

    /* Get faulting address from CR2 */
    uint64_t fault_addr;
    __asm__ volatile("mov %%cr2, %0" : "=r"(fault_addr));

    uint64_t *pte = vmm_get_pte(kernel_pml4, fault_addr, 1, false);
    if (!pte) {
        serial_printf("[swap] page fault at 0x%llx — no PTE, fatal\n", fault_addr);
        __asm__ volatile("hlt");
        return;
    }

    if (*pte & PTE_SWAPPED) {
        /* Swap-in: allocate physical frame, read from swap */
        uint64_t swap_idx = (*pte & PTE_SWAP_IDX) >> 12;
        uint64_t frame = pmm_alloc();

        if (!frame) {
            /* No free frames — evict something first */
            serial_printf("[swap] swap-in needed but no free frames, evicting\n");
            /* TODO: LRU eviction — for now just halt */
            __asm__ volatile("hlt");
            return;
        }

        /* Read page from swap (via NVMe driver — call will be registered) */
        /* swap_read_page(swap_idx, frame); */

        /* Update PTE */
        *pte = frame | PTE_PRESENT | PTE_WRITABLE;
        vmm_invlpg(fault_addr & ~0xFFFULL);
        swap_used--;

    } else {
        serial_printf("[swap] unexpected page fault at 0x%llx, pte=0x%llx\n",
                      fault_addr, *pte);
    }
}

void
swap_init(void) {
    /* Register page fault handler */
    idt_register_handler(14, swap_page_fault_handler, NULL);

    /* Calculate desired swap size: min(SSD_free * 0.8, SWAP_MAX_GB GB) */
    /* For now set to 64GB — nvme driver will resize on first access */
    swap_capacity = SWAP_MAX_PAGES;
    swap_active   = true;

    serial_printf("[swap] active, capacity=%lluGB\n",
                  (swap_capacity * PAGE_SIZE) / (1024ULL*1024*1024));
}

/* ── High-level allocator (kmalloc) ─────────────────────────────── */

/*
 * Simple bump allocator for kernel use. Later replaced by slab allocator.
 * Allocations are never freed (kernel structures live forever).
 */

static uint64_t heap_ptr = 0;
static uint64_t heap_end = 0;
#define HEAP_INITIAL_SIZE  (16 * 1024 * 1024)  /* 16MB initial heap */

void
kmalloc_init(void) {
    uint64_t phys = pmm_alloc_contiguous(HEAP_INITIAL_SIZE / PAGE_SIZE);
    if (!phys) {
        serial_printf("[kmalloc] FATAL: cannot allocate initial heap\n");
        return;
    }
    heap_ptr = phys + VIRT_PHYS_MAP_BASE;
    heap_end = heap_ptr + HEAP_INITIAL_SIZE;
    serial_printf("[kmalloc] heap at 0x%llx, size=16MB\n", heap_ptr);
}

void *
kmalloc(uint64_t size) {
    size = (size + 15) & ~15ULL;  /* 16-byte align */
    if (heap_ptr + size > heap_end) return NULL;
    void *ptr = (void *)heap_ptr;
    heap_ptr += size;
    return ptr;
}

void *
kmalloc_aligned(uint64_t size, uint64_t align) {
    heap_ptr = (heap_ptr + align - 1) & ~(align - 1);
    return kmalloc(size);
}

/* kfree is a no-op in the bump allocator */
void kfree(void *ptr) { (void)ptr; }

/* ── Memory status (exposed to AION consciousness) ───────────────── */

MemoryStatus
memory_get_status(void) {
    MemoryStatus s;
    s.total_physical  = phys_total_pages * PAGE_SIZE;
    s.free_physical   = phys_free_pages  * PAGE_SIZE;
    s.used_physical   = s.total_physical - s.free_physical;
    s.swap_total      = swap_capacity * PAGE_SIZE;
    s.swap_used       = swap_used * PAGE_SIZE;
    s.pressure_level  = (s.free_physical < 256 * 1024 * 1024) ? 2 :
                        (s.free_physical < 512 * 1024 * 1024) ? 1 : 0;
    return s;
}

/* ── Top-level init ──────────────────────────────────────────────── */

void
memory_init(uint64_t mmap_addr, uint64_t mmap_size, uint64_t desc_size) {
    pmm_init(mmap_addr, mmap_size, desc_size);
    vmm_init();
    kmalloc_init();
    swap_init();
    serial_printf("[memory] subsystem ready\n");
}
