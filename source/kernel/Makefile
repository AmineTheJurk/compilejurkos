CC ?= gcc
LD ?= ld
PYTHON ?= python3

CFLAGS_COMMON = -std=c11 -ffreestanding -fno-stack-protector -fno-pie -no-pie \
                -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
                -Wl,-z,noexecstack -Iinclude -O2

CFLAGS_64 = $(CFLAGS_COMMON) -m64 -mno-red-zone -mcmodel=kernel
CFLAGS_32 = $(CFLAGS_COMMON) -m32

SRCS_C = kernel_main.c \
         lib/string.c \
         mem/pmm.c \
         mem/kheap.c \
         drivers/serial.c \
         drivers/framebuffer.c \
         drivers/keyboard.c \
         drivers/pci.c \
         drivers/e1000.c \
         drivers/speaker.c \
         fs/vfs.c \
         fs/devfs.c \
         fs/jurkvfs.c \
         fs/fat32.c \
         loader/elf.c \
         loader/xec.c \
         loader/loader.c

SRCS_64_ARCH = arch/x86_64/entry.s \
               arch/x86_64/interrupts.s \
               arch/x86_64/gdt.c \
               arch/x86_64/idt.c \
               arch/x86_64/paging.c

SRCS_32_ARCH = arch/x86/entry.s \
               arch/x86/interrupts.s \
               arch/x86/gdt.c \
               arch/x86/idt.c \
               arch/x86/paging.c

all: kernel-x86_64.elf kernel-x86.elf

kernel-x86_64.elf: $(SRCS_C) $(SRCS_64_ARCH)
	@echo "===> Compiling JurkOS 64-bit Kernel (kernel-x86_64.elf)..."
	$(CC) $(CFLAGS_64) -T arch/x86_64/linker.ld -o $@ $^ -nostdlib
	@echo "===> 64-bit Kernel Build Successful: $@"

kernel-x86.elf: $(SRCS_C) $(SRCS_32_ARCH)
	@echo "===> Compiling JurkOS 32-bit Kernel (kernel-x86.elf)..."
	$(CC) $(CFLAGS_32) -T arch/x86/linker.ld -o $@ $^ -nostdlib
	@echo "===> 32-bit Kernel Build Successful: $@"

clean:
	rm -f kernel-x86_64.elf kernel-x86.elf *.o

.PHONY: all clean
