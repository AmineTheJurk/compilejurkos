[//]: # (Copyright (c) 2026 AmineTheJurk)
# JurkOS Compilation Guide

This document explains how to build the JurkOS release using **EXTLINUX** (Syslinux).

---

## How it Works
1. **Kernel Entry Point**: We compile `main.c` statically. This ensures the binary has zero dependencies and can run as PID 1 on the Linux kernel.
2. **Image Creation**: A raw 512MB disk image is created using `truncate`.
3. **Partitioning**: An MBR partition table is written to the image, and a primary `ext4` partition is created with boot flag enabled.
4. **Deterministic PARTUUID**: We use `dd` to inject Disk Signature `12345678` into the MBR so the OS mounts the exact root partition reliably.
5. **Filesystem & Injection**: The image is formatted as `ext4`, and all system files and binaries are copied to the root partition.
6. **Bootloader (EXTLINUX)**: EXTLINUX is installed into `/boot/extlinux` with `syslinux` MBR, providing a lean, low-RAM boot experience.

## 1. Install Required Tools
If compiling on Linux / CI:
```bash
sudo apt update && sudo apt install -y gcc-multilib extlinux syslinux syslinux-common parted e2fsprogs
```

## 2. Build the Image with EXTLINUX
Navigate to the project root and run:
```bash
gcc -static iso/sbin/main.c -o iso/sbin/main && \
gcc -static iso/sbin/jurkstore.c -o iso/bin/JurkStore && \
chmod +x iso/bin/JurkStore && ln -sf JurkStore iso/bin/jurkstore && \
gcc -static -O2 iso/sbin/wifi.c -o iso/bin/wifi && \
chmod +x iso/bin/wifi && cp iso/bin/wifi iso/sbin/wifi && \
truncate -s 512M jurkos.img && \
parted -s jurkos.img mklabel msdos mkpart primary ext4 1M 100% set 1 boot on && \
printf "\x78\x56\x34\x12" | dd of=jurkos.img bs=1 seek=440 conv=notrunc status=none && \
DEVICE=$(sudo losetup -Pf --show jurkos.img) && \
sudo mkfs.ext4 -L JURKOS_ROOT ${DEVICE}p1 && \
sudo mkdir -p /mnt/jurk_tmp && \
sudo mount ${DEVICE}p1 /mnt/jurk_tmp && \
sudo cp -a iso/. /mnt/jurk_tmp/ && \
sudo mkdir -p /mnt/jurk_tmp/boot/extlinux && \
sudo cp /mnt/jurk_tmp/boot/extlinux/extlinux.conf /mnt/jurk_tmp/boot/extlinux/ 2>/dev/null || true && \
sudo extlinux --install /mnt/jurk_tmp/boot/extlinux && \
sudo dd bs=440 count=1 conv=notrunc if=/usr/lib/syslinux/mbr/mbr.bin of="$DEVICE" 2>/dev/null || sudo dd bs=440 count=1 conv=notrunc if=/usr/lib/EXTLINUX/mbr.bin of="$DEVICE" && \
sudo umount /mnt/jurk_tmp && \
sudo losetup -d $DEVICE && \
echo "JurkOS: Build Success with EXTLINUX."
```

## 3. Build with Buildroot (Alternative Full-System Build)
JurkOS includes a pre-configured Buildroot configuration (`source/buildroot/buildroot.config` and defconfigs) with:
- Minimal BusyBox userspace
- Wireless networking stack (iw, wpa_supplicant, iwlist)
- Built-in kernel drivers for Ethernet and wireless network cards

To build the complete system with Buildroot:
```bash
git clone https://gitlab.com/buildroot.org/buildroot.git
cd buildroot
cp ../The-JurkOS-Project/source/buildroot/jurkos_x86_64_defconfig configs/
make jurkos_x86_64_defconfig
make -j$(nproc)
```

## 4. Flash to USB
Once the build is complete, use **ImageUSB** or `dd` to flash `jurkos.img` to your physical USB drive.

## 5. Testing in QEMU
To test your flashed physical USB drive in QEMU on Windows (Drive 1):
```cmd
qemu-system-x86_64 -m 2048M -smp 2 -drive file=\\.\PhysicalDrive1,format=raw -netdev user,id=net0,hostfwd=tcp::8080-:80 -device e1000,netdev=net0 -vga std -serial stdio
```

To test on Linux using Disk ID (`0x12345678`):
```bash
sudo qemu-system-x86_64 -m 2048M -smp 2 -drive file=/dev/disk/by-id/mbr-0x12345678,format=raw -netdev user,id=net0,hostfwd=tcp::8080-:80 -device e1000,netdev=net0 -vga std -serial stdio
```

