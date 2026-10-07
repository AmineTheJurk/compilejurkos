[//]: # (Copyright (c) 2026 AmineTheJurk)
# JurkOS Compilation Guide (FreeBSD Unix Kernel & Limine Bootloader)

This document explains how to compile JurkOS supporting both **64-bit (`kernel-x86_64`)** and **32-bit (`kernel-x86`)** FreeBSD Unix Kernels using the **Limine** bootloader.

---

## 1. Install Required Tools
```bash
pkg install -y gcc gmake limine e2fsprogs parted zip gzip
```

---

## 2. Compiling Kernel
Build the FreeBSD monolithic kernels (`JURKOS_FREEBSD_X86_64` and `JURKOS_FREEBSD_X86`):
```bash
# Compiling Kernel (64-bit & 32-bit)
cd source/freebsd
make -j$(nproc) KERNCONF=JURKOS_FREEBSD_X86_64
make -j$(nproc) KERNCONF=JURKOS_FREEBSD_X86
```

---

## 3. Compiling JurkOS
Compile userland binaries and construct the bootable image:
```bash
# Compiling JurkOS userland binaries
gcc -static iso/sbin/main.c -o iso/sbin/main
gcc -static iso/sbin/jurkstore.c -o iso/bin/JurkStore
chmod +x iso/bin/JurkStore && ln -sf JurkStore iso/bin/jurkstore
gcc -static -O2 iso/sbin/wifi.c -o iso/bin/wifi
chmod +x iso/bin/wifi && cp iso/bin/wifi iso/sbin/wifi

# Configure Limine bootloader & FreeBSD loader.conf
mkdir -p iso/boot/limine iso/boot/defaults
cp -f iso/boot/limine.conf iso/boot/limine/limine.conf
cp -f iso/boot/limine.cfg iso/boot/limine/limine.cfg

# Construct 512MB disk image with DISKUID 0x12345678
truncate -s 512M jurkos.img
parted -s jurkos.img mklabel msdos mkpart primary ext4 1M 100% set 1 boot on
printf "\x78\x56\x34\x12" | dd of=jurkos.img bs=1 seek=440 conv=notrunc status=none
limine bios-install jurkos.img

echo "Compiling JurkOS complete."
```

---

## 4. Testing in QEMU
```bash
qemu-system-x86_64 -m 2048M -smp 2 -drive file=jurkos.img,format=raw -netdev user,id=net0 -device e1000,netdev=net0 -vga std
```
