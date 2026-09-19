# JurkOS Buildroot Configuration & Guide

This directory contains the automated configurations for building JurkOS with **Buildroot** and hardware-accelerated OpenGL graphics via **libdrm**, **Mesa3D (Gallium)**, **EGL**, **GLES**, and **GBM**.

---

## 1. Enabled Configuration Options

### Target packages -> Graphic libraries and applications
- `[*]` **libdrm** (`BR2_PACKAGE_LIBDRM=y`)
  - `[*]` **intel** (`BR2_PACKAGE_LIBDRM_INTEL=y`)
  - `[*]` **radeon** (`BR2_PACKAGE_LIBDRM_RADEON=y`)
- `[*]` **mesa3d** (`BR2_PACKAGE_MESA3D=y`)
  - `[*]` **Enable Gallium driver** (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER=y`)
  - `[*]` **Enable OpenGL EGL** (`BR2_PACKAGE_MESA3D_OPENGL_EGL=y`)
  - `[*]` **Enable OpenGL ES** (`BR2_PACKAGE_MESA3D_OPENGL_ES=y`)
  - `[*]` **Enable GBM** (`BR2_PACKAGE_MESA3D_GBM=y`)
  - **Gallium drivers**:
    - `[*]` **swrast** (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_SWRAST=y`) - Software fallback rasterizer
    - `[*]` **virgl** (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_VIRGL=y`) - VirtIO 3D paravirtualized GPU
    - `[*]` **iris** (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_IRIS=y`) - Modern Intel Gen9+ (Skylake to present)
    - `[*]` **radeonsi** (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_RADEONSI=y`) - AMD Southern Islands & newer GCN/RDNA
    - `[*]` **nouveau** (`BR2_PACKAGE_MESA3D_GALLIUM_DRIVER_NOUVEAU=y`) - NVIDIA GeForce GPUs

---

### Linux Kernel -> Kernel configuration -> Device Drivers -> Graphics support
The underlying Linux kernel (`source/linux/64-bit/.config` and `source/linux/32-bit/.config`) has built-in hardware acceleration enabled:
- `[*]` **Direct Rendering Manager (XFree86 4.1.0 and higher) via DRI** (`CONFIG_DRM=y`, `CONFIG_DRM_KMS_HELPER=y`)
- `[*]` **Intel 8xx/9xx/G3x/G4x/HD Graphics** (`CONFIG_DRM_I915=y`)
- `[*]` **AMD GPU** (`CONFIG_DRM_AMDGPU=y`, `CONFIG_DRM_TTM=y`, `CONFIG_DRM_BUDDY=y`)
- `[*]` **Virtio GPU driver** (`CONFIG_DRM_VIRTIO_GPU=y`)

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
