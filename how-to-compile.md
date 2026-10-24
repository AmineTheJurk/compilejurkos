[//]: # (Copyright (c) 2026 AmineTheJurk)
# JurkOS Compilation Guide (Kernel & VFS)

This document explains how to compile the standalone **JurkOS Kernel** supporting both **64-bit (`kernel-x86_64.elf`)** and **32-bit (`kernel-x86.elf`)** with the standard **VFS** filesystem and Limine bootloader.

---

## 1. Install Required Tools
```bash
sudo apt update && sudo apt install -y build-essential gcc-multilib binutils limine zip gzip dosfstools mtools parted
```

---

## 2. Compiling Kernel
Build both 64-bit and 32-bit custom kernels:
```bash
cd source/kernel
make kernel-x86_64.elf
make kernel-x86.elf

# Copy kernel binaries to boot directory
mkdir -p ../../iso/boot/limine
cp kernel-x86_64.elf ../../iso/boot/limine/kernel-x86_64
cp kernel-x86.elf ../../iso/boot/limine/kernel-x86
cd ../..
echo "Compiling Kernel complete."
```

---

## 3. Compiling JurkOS
Compile userland binaries and generate the VFS disk image:
```bash
# Compile BearSSL and Bare-Metal wget
make -C source/bearssl -j4
mkdir -p lib && cp -f source/bearssl/build/libbearssl.a lib/libbearssl.a
gcc -static -O2 NoneCompiled/wget.c lib/libbearssl.a -Isource -o iso/bin/wget
chmod +x iso/bin/wget

# Compile userland applications from NoneCompiled/
gcc -static NoneCompiled/main.c -o iso/sbin/main
gcc -static NoneCompiled/jurkstore.c -o iso/bin/JurkStore
cp -f iso/bin/JurkStore iso/sbin/jurkstore
chmod +x iso/bin/JurkStore && ln -sf JurkStore iso/bin/jurkstore
gcc -static -O2 NoneCompiled/wifi.c -o iso/bin/wifi
cp -f iso/bin/wifi iso/sbin/wifi
chmod +x iso/bin/* iso/sbin/*

# Create .JEX package (renamed ELF / flat XEC)
cp -f iso/bin/JurkStore iso/bin/jurkstore.jex

# Build standard FAT32 disk image directly
rm -f jurkos.img
truncate -s 128M jurkos.img
parted -s jurkos.img mklabel msdos
parted -s jurkos.img mkpart primary fat32 1MiB 100%
parted -s jurkos.img set 1 boot on
printf "\x78\x56\x34\x12" | dd of=jurkos.img bs=1 seek=440 conv=notrunc status=none
mformat -i jurkos.img@@1M -F -v "JURKOS"
mcopy -o -s -i jurkos.img@@1M iso/* ::/

# Install the 512-byte BIOS MBR Stage 1 & Stage 2 bootloader
limine bios-install jurkos.img

# (Optional: on Windows using limine.exe)
# iso\boot\limine\limine.exe bios-install jurkos.img

echo "Compiling JurkOS complete."
```

---

## 4. Testing in QEMU (Instant Boot)
### Windows (with WHPX Acceleration & USB Emulation):
```cmd
qemu-system-x86_64 -accel whpx -cpu host -smp 1 -m 512M -device qemu-xhci -device usb-storage,drive=usbdrive -drive if=none,id=usbdrive,format=raw,file=jurkos.img,cache=unsafe -netdev user,id=net0 -device e1000,netdev=net0 -vga std -serial stdio
```

### Linux (with KVM Acceleration & USB Emulation):
```bash
qemu-system-x86_64 -enable-kvm -cpu host -smp 1 -m 512M -device qemu-xhci -device usb-storage,drive=usbdrive -drive if=none,id=usbdrive,format=raw,file=jurkos.img,cache=unsafe -netdev user,id=net0 -device e1000,netdev=net0 -vga std -serial stdio
```

