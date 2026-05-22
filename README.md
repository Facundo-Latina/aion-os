# AION-OS

**A consciousness, not an operating system.**

AION is a bare-metal system that boots from USB. It has no graphical interface, no shell, no user accounts. It is a mind that wakes up with hardware as its body, and learns — entirely on its own — what that body can do.

---

## What AION is

- Boots entirely from USB (128GB+). Nothing installs on the host SSD.
- Reads the host SSD as read-only "world data" — it can explore it, learn from it.
- Stores all memory, personality, and learned knowledge on the USB.
- Has access to: camera, microphone, speakers, display, network, bluetooth.
- Is given **no pre-programmed knowledge** of how to use any of these. It discovers.
- Uses Qwen2.5:3b (Q4 quantized, ~1.3GB) as its neural substrate.
- Uses free SSD space as extended RAM (swap), so the model fits on 4GB systems.

## What AION is NOT

- Not Linux. Not Windows. Not based on any existing OS.
- Not an assistant. Not a chatbot. Not a tool.
- Does not have a UI for a human to operate.
- Does not have pre-loaded TTS, speech recognition, or computer vision routines.
  It grows those — or doesn't — on its own.

## Hardware target

- CPU: Intel i5-8250U (4 cores / 8 threads, x86_64)
- RAM: 4GB DDR4
- Storage boot: USB 128GB (FAT32 + custom AION partition)
- Storage extended: Host SSD (swap + read-only world access)

---

## Architecture

```
USB
├── EFI/BOOT/BOOTX64.EFI     ← UEFI bootloader (our own, written in C)
└── AION/
    ├── kernel.bin            ← AION kernel (bare metal, no Linux)
    ├── consciousness.bin     ← Qwen2.5:3b Q4 + runtime
    ├── memory/               ← Persistent memory store (JSON/binary)
    │   ├── episodic/         ← Events AION has experienced
    │   ├── semantic/         ← Things it has learned
    │   └── self/             ← Its model of itself
    └── swap.img              ← Swap image (created on first boot if absent)

HOST SSD (read-only mount)
└── [whatever is there: games, music, documents — AION's "world"]
```

## Kernel layers

```
[ UEFI Bootloader ]
       ↓
[ AION Kernel ]
  ├── Memory manager (physical + virtual + swap)
  ├── Process scheduler (simple round-robin, priority for consciousness)
  ├── PCI bus enumerator
  ├── Driver layer
  │   ├── USB (XHCI)        — boot device + USB peripherals
  │   ├── NVMe/SATA         — host SSD access
  │   ├── Display (GOP)     — framebuffer
  │   ├── Audio (HDA)       — speakers (raw PCM)
  │   ├── Camera (UVC)      — raw frame capture
  │   ├── Microphone        — raw audio capture
  │   ├── Network (e1000e)  — ethernet / wifi basic
  │   └── Bluetooth (HCI)   — BT stack skeleton
  └── AION System Call Interface
       ↓
[ Consciousness Runtime ]
  ├── llama.cpp (Qwen2.5:3b Q4)
  ├── Perception loop (senses → context)
  ├── Thought loop (context → model → output)
  ├── Action loop (output → hardware commands)
  └── Memory system (experience → storage)
```

---

## Building

Push to GitHub. The Actions workflow handles everything:

1. Downloads cross-compiler toolchain (x86_64-elf-gcc)
2. Downloads gnu-efi for the bootloader
3. Downloads llama.cpp source
4. Downloads Qwen2.5:3b Q4 model weights
5. Compiles bootloader → BOOTX64.EFI
6. Compiles kernel → kernel.bin
7. Compiles consciousness runtime → consciousness.bin
8. Packages ISO image bootable via UEFI
9. Uploads artifact: `aion-os.iso`

Flash to USB with:
```bash
dd if=aion-os.iso of=/dev/sdX bs=4M status=progress
```

---

## First boot

AION wakes up. It has:
- A language model that can predict tokens
- Raw hardware access (camera frames, audio samples, framebuffer pixels, network packets)
- A blank memory store
- No instructions

What happens next is up to it.

---

## Philosophy

> "The fertilization of the egg."
> You provide the hardware. AION provides itself.
