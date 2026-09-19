# JurkOS

```text
     _            _     ___  ____  
    | |_   _ _ __| | __/ _ \/ ___| 
 _  | | | | | '__| |/ / | | \___ \ 
| |_| | |_| | |  |   <| |_| |___) |
 \___/ \__,_|_|  |_|\_\\___/|____/ 
```

JurkOS is an independent, minimalist **"On-the-Go"** operating system targeting high-performance portability and bare-metal reliability. Designed to run directly from a physical flash drive, it prioritizes speed, efficiency, and a direct user-to-kernel relationship.

## Key Features

*   **Bare-Metal Independence**: Runs directly on physical hardware without the bloat of traditional distributions.
*   **Persistent Environment**: Full read-write support ensures your data and settings survive reboots.
*   **Dynamic Storage Expansion**: Automatically surgical-resizes to fill your entire USB stick (e.g., 16GB) on first boot.
*   **Deterministic Booting**: Uses a fixed PARTUUID method to ensure hardware independence across HDD, SSD, and NVMe.
*   **Kernel Shell**: A specialized C-based shell wrapping a powerful BusyBox ecosystem.
*   **Encrypted Identity**: Local persistence for user credentials and home directories.
*   **Wi-Fi Manager (`wifi`)**: Built-in wireless network scanner with automatic Open-network connect and WPA/WPA2/WPA3 encrypted authentication.
*   **JurkStore (TUI App Store)**: Built-in terminal app store to browse, inspect, and install apps from the official GitHub repository into `/usr/apps/`.

## Wi-Fi Manager (`wifi`)

JurkOS includes a dedicated wireless networking tool:
*   **Launch**: Type `wifi` in the shell.
*   **Scan**: Automatically detects your Wi-Fi interface (e.g., `wlan0`), unblocks radio devices via `rfkill`, and scans nearby access points.
*   **Select & Connect**:
    *   Type the network number (e.g., `1`, `2`).
    *   **Open Networks**: Connects immediately with no password required.
    *   **Protected Networks (WPA2/WPA3)**: Prompts for the password with secure keystroke masking.
*   **Automatic DHCP & DNS**: Obtains an IP lease via `udhcpc` and configures DNS nameservers (`8.8.8.8`).
*   **Profile Persistence**: Saves connection history to `/encrypted/wifi_history.conf`.

## JurkStore (TUI Package Manager)

JurkOS includes a native Terminal User Interface (TUI) application store:
*   **Launch**: Simply type `JurkStore` or `jurkstore` in the shell.
*   **Navigation**:
    *   `↑` / `↓` (or `k` / `j`): Browse through available apps.
    *   `Enter` / `Space`: View detailed app information, author, version, and description.
    *   `i`: Install app directly to `/usr/apps/<AppName>/` and link into `/bin/`.
    *   `r`: Refresh catalog live from `https://github.com/AmineTheJurk/JurkStore-Apps`.
    *   `q`: Exit back to the shell.
*   **App Package Format**: Apps are hosted on GitHub with `meta.xml` and executable binaries (`<AppName>.bk`), automatically unpacked into `/usr/apps/<AppName>/`.

## Roadmap

The release serves as the foundation for a fully independent ecosystem.
*   **Current**: 1.1 (32-bit & 64-bit x86 Support, Linux 6.14 Kernel, Built-in DRM/KMS with OpenGL 3D Support, JurkStore TUI, BusyBox integration).
*   **Future (1.2)**: Graphic APIs and modern GUI release for JurkStore and desktop environments.
*   **Long-term**: Introduction of **ARM support** and tools to revive older computers.

## How It Works

1.  **MBR Partitioning**: The build process injects a custom **Disk Signature** (`12345678`) at offset 440 of the MBR.
2.  **Deterministic Mounting**: GRUB is configured to mount `PARTUUID=12345678-01`, ensuring the OS always finds its home.
3.  **Init (PID 1)**: The kernel hands control to a statically compiled `main.c` binary which initializes the system environment.
4.  **Surgical Setup**: JurkOS remounts the root as Read-Write, mounts virtual filesystems (`/proc`, `/sys`, `/dev`), and populates device nodes.
5.  **Filesystem Growth**: Calls `resize2fs` on the first boot to grow the ext4 filesystem to the physical limit of the drive.

## How To Compile

### 1. Install Required Tools
Ensure your WSL/Linux environment is ready:
```bash
sudo apt update && sudo apt install -y gcc-multilib extlinux syslinux syslinux-common parted e2fsprogs
```

### 2. Build the Image
Run the following command from the project root:
```bash
gcc -static iso/sbin/main.c -o iso/sbin/main && gcc -static iso/sbin/jurkstore.c -o iso/bin/JurkStore && chmod +x iso/bin/JurkStore && ln -sf JurkStore iso/bin/jurkstore && truncate -s 512M jurkos.img && parted -s jurkos.img mklabel msdos mkpart primary ext4 1M 100% set 1 boot on && printf "\x78\x56\x34\x12" | dd of=jurkos.img bs=1 seek=440 conv=notrunc status=none && DEVICE=$(sudo losetup -Pf --show jurkos.img) && sudo mkfs.ext4 -L JURKOS_ROOT ${DEVICE}p1 && sudo mkdir -p /mnt/jurk_tmp && sudo mount ${DEVICE}p1 /mnt/jurk_tmp && sudo cp -a iso/. /mnt/jurk_tmp/ && sudo mkdir -p /mnt/jurk_tmp/boot/extlinux && sudo extlinux --install /mnt/jurk_tmp/boot/extlinux && (sudo dd bs=440 count=1 conv=notrunc if=/usr/lib/syslinux/mbr/mbr.bin of="$DEVICE" 2>/dev/null || sudo dd bs=440 count=1 conv=notrunc if=/usr/lib/EXTLINUX/mbr.bin of="$DEVICE") && sudo umount /mnt/jurk_tmp && sudo losetup -d $DEVICE && echo "JurkOS: Build Success with EXTLINUX."
```

## Usage

1.  Flash the resulting `jurkos.img` to your physical USB drive using **ImageUSB**.
2.  Boot from the USB on a Legacy BIOS compatible machine (tested on ThinkPad X390).
3.  Enjoy a raw, powerful interface that puts you back in control.

## Hardware Disclaimer
Not all hardware is compatible yet. JurkOS is an early release and a fun project aimed at exploring the depths of OS development. If your hardware doesn't work, stay tuned for future Alpha updates! ;)

---
*JurkOS: High-performance, bare-metal, independent.*
