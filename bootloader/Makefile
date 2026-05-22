CC      = x86_64-w64-mingw32-gcc
CFLAGS  = -Wall -Wextra -ffreestanding -fno-stack-protector \
          -fno-stack-check -fshort-wchar -mno-red-zone \
          -DGNU_EFI_USE_MS_ABI \
          -I/usr/include/efi \
          -I/usr/include/efi/x86_64 \
          -I/usr/include/efi/protocol
LDFLAGS = -nostdlib -znocombreloc -T /usr/lib/elf_x86_64_efi.lds \
          -shared -Bsymbolic /usr/lib/crt0-efi-x86_64.o

LIBS    = -lefi -lgnuefi

all: BOOTX64.EFI

bootloader.o: bootloader.c
	$(CC) $(CFLAGS) -c -o $@ $<

BOOTX64.so: bootloader.o
	$(CC) $(LDFLAGS) -o $@ $< $(LIBS)

BOOTX64.EFI: BOOTX64.so
	objcopy -j .text -j .sdata -j .data -j .rodata -j .dynamic \
	        -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* \
	        -j .reloc --strip-debug --target=efi-app-x86_64 $< $@

clean:
	rm -f *.o *.so *.EFI
