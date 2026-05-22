##
## AION-OS Kernel Makefile
##

CC      = x86_64-elf-gcc
AS      = nasm
LD      = x86_64-elf-ld

CFLAGS  = -std=c11 -ffreestanding -fno-stack-protector -fno-stack-check \
          -fno-pie -fno-pic -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
          -Wall -Wextra -Wno-unused-parameter \
          -O2 \
          -I. -Iinclude \
          -Idrivers/pci -Idrivers/display -Idrivers/usb -Idrivers/nvme \
          -Idrivers/audio -Idrivers/cam -Idrivers/mic -Idrivers/network \
          -Ifs -Iprocess -Isyscall -Imemory -Iarch/x86_64 \
          -I../consciousness/core -I../consciousness/memory \
          -I../consciousness/senses -I../consciousness/runtime \
          $(EXTRA_CFLAGS)

ASFLAGS = -f elf64

LDFLAGS = -T linker.ld -nostdlib -z max-page-size=0x1000

LLAMA_LIB ?= ../tools/llama.cpp/build/libllama.a

ASM_SRCS = arch/asm/entry.asm

C_SRCS = kernel.c \
         arch/x86_64/gdt.c \
         arch/x86_64/idt.c \
         memory/memory.c \
         drivers/serial.c \
         drivers/display/display.c \
         drivers/pci/pci.c \
         drivers/usb/usb.c \
         drivers/nvme/nvme.c \
         drivers/audio/audio.c \
         drivers/cam/cam.c \
         drivers/mic/mic.c \
         drivers/network/network.c \
         fs/fs.c \
         process/scheduler.c \
         syscall/syscall.c

CONSCIOUSNESS_C = \
         ../consciousness/core/consciousness.c \
         ../consciousness/core/action.c \
         ../consciousness/memory/aion_memory.c \
         ../consciousness/senses/senses.c \
         ../consciousness/runtime/llm.c

ASM_OBJS = $(ASM_SRCS:.asm=.o)
C_OBJS   = $(C_SRCS:.c=.o)
CON_OBJS = $(CONSCIOUSNESS_C:.c=.o)
ALL_OBJS = $(ASM_OBJS) $(C_OBJS) $(CON_OBJS)

.PHONY: all clean

all: kernel.bin

kernel.bin: $(ALL_OBJS) $(LLAMA_LIB)
	$(LD) $(LDFLAGS) -o $@ $^

arch/asm/entry.o: arch/asm/entry.asm
	$(AS) $(ASFLAGS) -o $@ $

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $

../consciousness/%.o: ../consciousness/%.c
	$(CC) $(CFLAGS) -c -o $@ $

clean:
	find . -name '*.o' -delete
	find ../consciousness -name '*.o' -delete
	rm -f kernel.bin
