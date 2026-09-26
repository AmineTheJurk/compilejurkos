# JurkOS Buildroot Configuration & Guide

This directory contains the automated configurations for building JurkOS with **Buildroot** and wireless networking tools (`wpa_supplicant`, `iw`, `iwlist`).

---

## 1. Enabled Configuration Options

### Target packages -> Networking applications
- `[*]` **wpa_supplicant**
- `[*]` **wireless-tools / iw**
- `[*]` **BusyBox minimalist system utilities**

---

## 2. How to Build with Buildroot

```bash
# 1. Download Buildroot
git clone https://gitlab.com/buildroot.org/buildroot.git
cd buildroot

# 2. Copy the JurkOS configuration
cp ../The-JurkOS-Project/source/buildroot/jurkos_x86_64_defconfig configs/

# 3. Apply defconfig
make jurkos_x86_64_defconfig

# 4. (Optional) Inspect or modify configuration
make menuconfig

# 5. Build rootfs, libraries, and kernel
make -j$(nproc)
```

The compiled rootfs image, libdrm, and Mesa3D Gallium shared objects (`libEGL.so`, `libGLESv2.so`, `libgbm.so`, `libGL.so`, and Gallium driver dynamic libraries) will be located in `output/images/` and `output/target/usr/lib/`.
