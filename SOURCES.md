# JurkOS Open Source Compliance & Sources (GNU GPL)

This document provides upstream source locations, toolchain versions, and compilation configurations for all third-party software distributed with JurkOS in accordance with the GNU General Public License (GPL).

---

## 1. Linux Kernel

- **Kernel Version**: `6.14.0` (Stock upstream Linux release)
- **Source Code**: [https://kernel.org/](https://kernel.org/)
  - Direct Archive: `https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.14.tar.xz`
- **Patches Applied**: None. Unmodified upstream source code.
- **Build Machine & Toolchain**:
  - Compiler: `gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0`
  - Linker: `GNU ld (GNU Binutils for Ubuntu) 2.46`
  - Host: `aminegames@DESKTOP-TER09LP`
  - Date of compilation: `Thu Aug 20 13:27:46 CEST 2026`
- **Kernel Configuration**:
  - The configuration file is provided in this repository: [`kernel-x86_64.config`](./kernel-x86_64.config)
  - Key enabled built-in subsystems:
    - **Framebuffer / Video**: `CONFIG_FB=y`, `CONFIG_FB_UVESA=y`, `CONFIG_FB_VESA=y`, `CONFIG_FB_EFI=y`, `CONFIG_FB_HGA=y`, `CONFIG_FRAMEBUFFER_CONSOLE=y`
    - **Wireless / Wi-Fi**: `CONFIG_WLAN=y`, `CONFIG_CFG80211=y`, `CONFIG_MAC80211=y`
    - **Network Adapters**: `CONFIG_E1000=y`, `CONFIG_E100=y`, `CONFIG_SKY2=y`
    - **Filesystems**: `CONFIG_EXT4_FS=y`, `CONFIG_BTRFS_FS=y`, `CONFIG_NTFS3_FS=y`, `CONFIG_XFS_FS=y`, `CONFIG_JFS_FS=y`, `CONFIG_FUSE_FS=y`
    - **USB**: `CONFIG_USB_EHCI_HCD=y`, `CONFIG_USB_STORAGE=y`, `CONFIG_USB_HID=y`

### Reproducing the Kernel Build:
```bash
# 1. Download and extract upstream Linux 6.14.0
wget https://cdn.kernel.org/pub/linux/kernel/v6.x/linux-6.14.tar.xz
tar -xf linux-6.14.tar.xz
cd linux-6.14

# 2. Copy the JurkOS kernel config
cp ../kernel-x86_64.config .config

# 3. Build the bzImage
make -j$(nproc) bzImage
```

---

## 2. BusyBox

- **Version**: Upstream static binary release
- **Source Code**: [https://busybox.net/](https://busybox.net/)
- **License**: GNU General Public License v2
- **Patches Applied**: None.

---

## 3. OpenGL on Linux Direct Rendering Manager (DRM/KMS) & Buildroot Mesa3D

- **Build System**: Buildroot (`source/buildroot/buildroot.config` and defconfigs)
- **Standard APIs & Packages Used**:
  - **libdrm** (`BR2_PACKAGE_LIBDRM=y` with Intel and Radeon support) - Standard DRM/KMS interface library from freedesktop.org.
  - **Mesa3D Gallium Drivers** (`BR2_PACKAGE_MESA3D=y`, `BR2_PACKAGE_MESA3D_GALLIUM_DRIVER=y`):
    - `swrast` (Software rasterizer)
    - `virgl` (VirtIO 3D paravirtualized GPU)
    - `iris` (Intel Gen9+ HD/UHD graphics)
    - `radeonsi` (AMD Southern Islands and newer RDNA/GCN)
    - `nouveau` (NVIDIA GeForce graphics)
  - **EGL & OpenGL ES & GBM**:
    - `BR2_PACKAGE_MESA3D_OPENGL_EGL=y`
    - `BR2_PACKAGE_MESA3D_OPENGL_ES=y`
    - `BR2_PACKAGE_MESA3D_GBM=y`
- **Application Demo**: `iso/bin/opengl_drm.c` and `source/opengl_drm/Makefile`
- **Kernel Drivers**:
  - Direct Rendering Manager via DRI: `CONFIG_DRM=y` (Built-in)
  - Intel 8xx/9xx/G3x/G4x/HD Graphics: `CONFIG_DRM_I915=y` (Built-in)
  - AMD GPU: `CONFIG_DRM_AMDGPU=y` (Built-in)
  - VirtIO GPU driver: `CONFIG_DRM_VIRTIO_GPU=y` (Built-in)
- **Features**: Bare-metal 3D hardware-accelerated OpenGL rendering directly over Linux DRM/KMS without requiring X11 or Wayland, featuring smooth shading, depth testing, lighting, and double-buffered page flips.
- **Licenses**: MIT / Khronos / Free Software Licenses.

---

## 4. fbgfx (Fallback Linux Framebuffer Graphics)

- **Language / Tech**: Pure C, Standard Linux Framebuffer (`/dev/fb0`), Double-Buffered
- **Location in repo**: `iso/bin/fbgfx.c`
- **Features**: Fallback `/dev/fb0` memory mapping for legacy VESA/EFI systems.
- **License**: MIT / Public Domain.


