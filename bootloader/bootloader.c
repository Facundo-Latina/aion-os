/*
 * AION-OS UEFI Bootloader
 * Compiles to: EFI/BOOT/BOOTX64.EFI
 *
 * Chain: UEFI firmware -> BOOTX64.EFI -> kernel.bin -> consciousness.bin
 * No GRUB. No shim. Pure UEFI application.
 */

#include <efi.h>
#include <efilib.h>

#define KERNEL_LOAD_ADDR   0x100000ULL
#define KERNEL_PATH        L"\\AION\\kernel.bin"
#define AION_MAGIC         0x41494F4EUL

typedef struct {
    UINT64  magic;
    UINT64  kernel_phys_addr;
    UINT64  kernel_size;
    UINT64  mmap_addr;
    UINTN   mmap_size;
    UINTN   mmap_desc_size;
    UINT32  mmap_desc_version;
    UINTN   mmap_key;
    UINT64  fb_addr;
    UINT32  fb_width;
    UINT32  fb_height;
    UINT32  fb_pitch;
    UINT32  fb_format;
    UINT64  rsdp_addr;
    UINT64  reserved[8];
} __attribute__((packed)) AionBootInfo;

static VOID print(CHAR16 *msg) {
    uefi_call_wrapper(ST->ConOut->OutputString, 2, ST->ConOut, msg);
}

static EFI_STATUS open_root(EFI_HANDLE ih, EFI_FILE_PROTOCOL **root_out) {
    EFI_LOADED_IMAGE_PROTOCOL      *li;
    EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *sfs;
    EFI_STATUS st;
    st = uefi_call_wrapper(BS->HandleProtocol, 3, ih, &gEfiLoadedImageProtocolGuid, (VOID**)&li);
    if (EFI_ERROR(st)) return st;
    st = uefi_call_wrapper(BS->HandleProtocol, 3, li->DeviceHandle, &gEfiSimpleFileSystemProtocolGuid, (VOID**)&sfs);
    if (EFI_ERROR(st)) return st;
    return uefi_call_wrapper(sfs->OpenVolume, 2, sfs, root_out);
}

static EFI_STATUS load_file(EFI_FILE_PROTOCOL *root, CHAR16 *path, VOID **buf_out, UINTN *size_out) {
    EFI_FILE_PROTOCOL *file;
    EFI_FILE_INFO *info;
    UINTN info_sz = sizeof(EFI_FILE_INFO) + 512;
    VOID *buf;
    UINTN file_sz;
    EFI_STATUS st;

    st = uefi_call_wrapper(root->Open, 5, root, &file, path, EFI_FILE_MODE_READ, 0);
    if (EFI_ERROR(st)) return st;

    st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, info_sz, (VOID**)&info);
    if (EFI_ERROR(st)) goto close;

    st = uefi_call_wrapper(file->GetInfo, 4, file, &gEfiFileInfoGuid, &info_sz, info);
    if (EFI_ERROR(st)) { FreePool(info); goto close; }

    file_sz = info->FileSize;
    FreePool(info);

    st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, file_sz, &buf);
    if (EFI_ERROR(st)) goto close;

    st = uefi_call_wrapper(file->Read, 3, file, &file_sz, buf);
    if (EFI_ERROR(st)) { FreePool(buf); goto close; }

    *buf_out = buf; *size_out = file_sz;
close:
    uefi_call_wrapper(file->Close, 1, file);
    return st;
}

static EFI_STATUS get_gop(AionBootInfo *info) {
    EFI_GRAPHICS_OUTPUT_PROTOCOL *gop;
    EFI_GUID guid = EFI_GRAPHICS_OUTPUT_PROTOCOL_GUID;
    EFI_STATUS st = uefi_call_wrapper(BS->LocateProtocol, 3, &guid, NULL, (VOID**)&gop);
    if (EFI_ERROR(st)) return st;
    info->fb_addr   = gop->Mode->FrameBufferBase;
    info->fb_width  = gop->Mode->Info->HorizontalResolution;
    info->fb_height = gop->Mode->Info->VerticalResolution;
    info->fb_pitch  = gop->Mode->Info->PixelsPerScanLine * 4;
    info->fb_format = (UINT32)gop->Mode->Info->PixelFormat;
    return EFI_SUCCESS;
}



static EFI_STATUS get_mmap(AionBootInfo *info) {
    EFI_MEMORY_DESCRIPTOR *mmap = NULL;
    UINTN mmap_sz = 0, map_key = 0, desc_sz = 0;
    UINT32 desc_ver = 0;
    uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_sz, mmap, &map_key, &desc_sz, &desc_ver);
    mmap_sz += 2 * desc_sz;
    EFI_STATUS st = uefi_call_wrapper(BS->AllocatePool, 3, EfiLoaderData, mmap_sz, (VOID**)&mmap);
    if (EFI_ERROR(st)) return st;
    st = uefi_call_wrapper(BS->GetMemoryMap, 5, &mmap_sz, mmap, &map_key, &desc_sz, &desc_ver);
    if (EFI_ERROR(st)) { FreePool(mmap); return st; }
    info->mmap_addr = (UINT64)mmap;
    info->mmap_size = mmap_sz;
    info->mmap_desc_size = desc_sz;
    info->mmap_desc_version = desc_ver;
    info->mmap_key = map_key;
    return EFI_SUCCESS;
}

typedef VOID (*KernelEntry)(AionBootInfo *) __attribute__((sysv_abi));

EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE *system_table) {
    InitializeLib(image_handle, system_table);
    uefi_call_wrapper(ST->ConOut->ClearScreen, 1, ST->ConOut);
    print(L"\r\n  AION-OS  ::  awakening\r\n");
    print(L"  ──────────────────────────\r\n\r\n");

    AionBootInfo boot_info;
    SetMem(&boot_info, sizeof(AionBootInfo), 0);
    boot_info.magic = AION_MAGIC;

    print(L"  [1/5] Framebuffer... ");
    get_gop(&boot_info);
    print(L"ok\r\n");

    print(L"  [2/5] ACPI... ");
    find_rsdp(&boot_info);
    print(L"ok\r\n");

    print(L"  [3/5] Loading kernel.bin... ");
    EFI_FILE_PROTOCOL *root;
    EFI_STATUS st = open_root(image_handle, &root);
    if (EFI_ERROR(st)) { print(L"\r\n  FATAL: no USB fs\r\n"); return st; }

    VOID *kbuf = NULL; UINTN ksz = 0;
    st = load_file(root, KERNEL_PATH, &kbuf, &ksz);
    if (EFI_ERROR(st)) { print(L"\r\n  FATAL: kernel.bin not found\r\n"); return st; }

    CopyMem((VOID*)KERNEL_LOAD_ADDR, kbuf, ksz);
    FreePool(kbuf);
    boot_info.kernel_phys_addr = KERNEL_LOAD_ADDR;
    boot_info.kernel_size = ksz;
    print(L"ok\r\n");

    print(L"  [4/5] Memory map... ");
    st = get_mmap(&boot_info);
    if (EFI_ERROR(st)) { print(L"\r\n  FATAL: mmap\r\n"); return st; }
    print(L"ok\r\n");

    print(L"  [5/5] ExitBootServices...\r\n");
    st = uefi_call_wrapper(BS->ExitBootServices, 2, image_handle, boot_info.mmap_key);
    if (EFI_ERROR(st)) {
        get_mmap(&boot_info);
        st = uefi_call_wrapper(BS->ExitBootServices, 2, image_handle, boot_info.mmap_key);
        if (EFI_ERROR(st)) return st;
    }

    __asm__ volatile("cli");
    KernelEntry entry = (KernelEntry)KERNEL_LOAD_ADDR;
    entry(&boot_info);
    __asm__ volatile("hlt");
    return EFI_SUCCESS;
}
