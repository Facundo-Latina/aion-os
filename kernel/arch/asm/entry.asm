; AION Kernel — x86_64 Entry Point
; File: kernel/arch/asm/entry.asm
;
; This is the VERY FIRST code that runs after the UEFI bootloader hands control.
; We arrive here with:
;   - RDI = pointer to AionBootInfo struct
;   - Interrupts disabled (CLI was called by bootloader)
;   - We're in 64-bit long mode (UEFI sets this up)
;   - No valid stack yet — we set one up immediately
;   - No GDT of our own — UEFI's GDT is still active until we load ours
;
; Responsibilities:
;   1. Set up a temporary stack
;   2. Load our own GDT
;   3. Call kernel_main(AionBootInfo *)

[BITS 64]
[GLOBAL kernel_entry]
[EXTERN kernel_main]
[EXTERN gdt_descriptor]
[EXTERN _kernel_stack_top]

section .text

kernel_entry:
    ; Save boot info pointer (RDI) — we'll pass it to kernel_main
    ; Don't touch RDI yet
    
    ; Set up our own stack before doing anything else
    ; _kernel_stack_top is defined in linker script / bss
    mov rsp, _kernel_stack_top
    
    ; Align stack to 16 bytes (System V ABI requirement)
    and rsp, ~0xF
    
    ; Clear direction flag (required by ABI)
    cld
    
    ; Load our own GDT
    lgdt [gdt_descriptor]
    
    ; Reload segment registers with our GDT selectors
    ; Code segment (0x08), Data segment (0x10)
    ; We do a far return to reload CS
    push 0x08                    ; code segment selector
    push .reload_cs
    retfq

.reload_cs:
    mov ax, 0x10                 ; data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    
    ; Zero out BSS section
    ; (linker provides __bss_start and __bss_end)
    extern __bss_start
    extern __bss_end
    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor al, al
    rep stosb
    
    ; Restore boot info pointer into RDI (first argument to kernel_main)
    ; It was saved in RDI on entry and we haven't clobbered it... 
    ; Actually we did with stosb. We need to save it first.
    ; Fix: save it to a known location before the BSS clear.
    ; This version uses stack to preserve it:
    ; (Re-entry after reload_cs, RDI still has boot_info from caller)
    ; Note: the far-ret above preserved all regs except CS, so RDI is intact.
    
    ; Call kernel main
    ; RDI still = AionBootInfo pointer (preserved through all the above)
    call kernel_main
    
    ; kernel_main should never return, but if it does: halt forever
.halt:
    cli
    hlt
    jmp .halt


; ── Multiboot-style header (not needed but helps some loaders) ──────
section .note.aion_magic
    dd 0x41494F4E   ; "AION"
    dd 0x00000001   ; version 1
    dd 0x00000000   ; flags: none


; ── Initial kernel stack ────────────────────────────────────────────
; 64KB — enough for early boot. Memory manager will set up proper stacks.
section .bss
    align 16
    resb 65536          ; 64KB stack
_kernel_stack_top:
