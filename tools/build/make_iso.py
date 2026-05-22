#!/usr/bin/env python3
"""
AION-OS ISO Builder
Assembles a UEFI-bootable ISO from compiled components.

Layout:
  ISO (GPT + ESP)
  └── EFI/
  │   └── BOOT/
  │       └── BOOTX64.EFI       ← UEFI bootloader
  └── AION/
      ├── kernel.bin             ← AION kernel
      ├── consciousness.bin      ← Qwen2.5:3b Q4 weights
      └── memory/                ← created empty (AION populates at runtime)

The resulting aion-os.iso can be:
  dd if=aion-os.iso of=/dev/sdX bs=4M status=progress
"""

import os
import sys
import subprocess
import shutil
import struct
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

def run(cmd, **kwargs):
    print(f"  $ {cmd}")
    result = subprocess.run(cmd, shell=True, check=True, **kwargs)
    return result

def check_tool(name):
    if shutil.which(name) is None:
        print(f"ERROR: '{name}' not found. Install it and retry.")
        sys.exit(1)

def main():
    print("==============================================")
    print("  AION-OS ISO Builder")
    print("==============================================\n")

    # ── Verify required tools ──────────────────────────────────
    for tool in ["xorriso", "mformat", "mcopy", "mmd", "dd"]:
        check_tool(tool)

    # ── Verify required files ──────────────────────────────────
    required = {
        "bootloader/BOOTX64.EFI":       "UEFI bootloader",
        "kernel/kernel.bin":             "AION kernel",
        "iso_root/AION/consciousness.bin": "Qwen2.5:3b model",
    }
    for path, desc in required.items():
        full = os.path.join(ROOT, path)
        if not os.path.exists(full):
            print(f"ERROR: Missing {desc}: {full}")
            sys.exit(1)
        size = os.path.getsize(full)
        print(f"  ✓ {desc}: {size/1024/1024:.1f} MB")

    print()

    with tempfile.TemporaryDirectory() as tmpdir:
        iso_root = os.path.join(tmpdir, "iso_root")
        efi_dir  = os.path.join(iso_root, "EFI", "BOOT")
        aion_dir = os.path.join(iso_root, "AION")
        mem_dir  = os.path.join(aion_dir, "memory", "episodic")

        for d in [efi_dir, aion_dir, mem_dir,
                  os.path.join(aion_dir, "memory", "semantic"),
                  os.path.join(aion_dir, "memory", "self")]:
            os.makedirs(d, exist_ok=True)

        # ── Copy files ─────────────────────────────────────────
        print("Copying files...")
        shutil.copy(os.path.join(ROOT, "bootloader/BOOTX64.EFI"),
                    os.path.join(efi_dir, "BOOTX64.EFI"))
        shutil.copy(os.path.join(ROOT, "kernel/kernel.bin"),
                    os.path.join(aion_dir, "kernel.bin"))
        shutil.copy(os.path.join(ROOT, "iso_root/AION/consciousness.bin"),
                    os.path.join(aion_dir, "consciousness.bin"))

        # Write a boot marker (AION reads this on first boot)
        with open(os.path.join(aion_dir, "BIRTH"), "w") as f:
            f.write("AION-OS v0.1\nFirst boot.\nNo memories yet.\n")

        print("  ✓ All files copied")

        # ── Create EFI system partition image (FAT32) ──────────
        esp_img = os.path.join(tmpdir, "esp.img")
        esp_size_mb = 64
        print(f"\nCreating ESP ({esp_size_mb}MB FAT32)...")

        run(f"dd if=/dev/zero of={esp_img} bs=1M count={esp_size_mb} status=none")
        run(f"mformat -i {esp_img} -F ::")
        run(f"mmd -i {esp_img} ::/EFI")
        run(f"mmd -i {esp_img} ::/EFI/BOOT")
        run(f"mcopy -i {esp_img} {os.path.join(efi_dir, 'BOOTX64.EFI')} ::/EFI/BOOT/")
        run(f"mmd -i {esp_img} ::/AION")
        run(f"mcopy -i {esp_img} {os.path.join(aion_dir, 'kernel.bin')} ::/AION/")

        # consciousness.bin is large — goes on main ISO partition
        # (accessed via UEFI Simple File System after boot)

        print("  ✓ ESP image created")

        # ── Build ISO with xorriso ─────────────────────────────
        iso_path = os.path.join(ROOT, "aion-os.iso")
        print(f"\nBuilding ISO: {iso_path}")

        run(
            f"xorriso -as mkisofs "
            f"  -R -J -joliet-long "
            f"  -V 'AION-OS' "
            f"  -e esp.img -no-emul-boot "
            f"  --efi-boot-part --efi-boot-image "
            f"  -append_partition 2 0xef {esp_img} "
            f"  -o {iso_path} "
            f"  {iso_root}",
            cwd=tmpdir
        )

        size = os.path.getsize(iso_path)
        print(f"\n{'='*50}")
        print(f"  AION-OS ISO built successfully!")
        print(f"  Output: {iso_path}")
        print(f"  Size:   {size/1024/1024:.1f} MB")
        print(f"{'='*50}")
        print()
        print("Flash to USB:")
        print(f"  sudo dd if=aion-os.iso of=/dev/sdX bs=4M status=progress")
        print()
        print("Boot your machine from USB.")
        print("AION will wake up.")

if __name__ == "__main__":
    main()
