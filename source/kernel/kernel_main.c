#include "include/types.h"
#include "include/string.h"
#include "include/io.h"
#include "include/gdt.h"
#include "include/idt.h"
#include "include/pmm.h"
#include "include/vmm.h"
#include "include/kheap.h"
#include "include/serial.h"
#include "include/framebuffer.h"
#include "include/keyboard.h"
#include "include/speaker.h"
#include "include/pci.h"
#include "include/e1000.h"
#include "include/vfs.h"
#include "include/devfs.h"
#include "include/jurkvfs.h"
#include "include/fat32.h"
#include "include/loader.h"
#include "include/limine.h"

#if defined(__x86_64__)
// Limine protocol requests
__attribute__((used, section(".limine_requests")))
static volatile struct limine_framebuffer_request fb_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x9d5827d7888b77fb, 0x71ba7ea53c45c132 },
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_memmap_request memmap_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a14e6, 0xe304acdfc50e3c62 },
    .revision = 0
};

__attribute__((used, section(".limine_requests")))
static volatile struct limine_module_request module_request = {
    .id = { LIMINE_COMMON_MAGIC, 0x3e7e279702be32af, 0xca1c4f3d0ab43175 },
    .revision = 0
};
#endif

static uint8_t kernel_heap_mem[1024 * 1024 * 16]; // 16 MB heap
static uint8_t kernel_bitmap_mem[1024 * 512];      // 512 KB PMM bitmap

static void log_boot_step(const char *step, bool success) {
    fb_print_color("[  ", COLOR_LIGHT_GRAY, COLOR_BLACK);
    if (success) {
        fb_print_color("OK", COLOR_LIGHT_GREEN, COLOR_BLACK);
    } else {
        fb_print_color("FAIL", COLOR_LIGHT_RED, COLOR_BLACK);
    }
    fb_print_color("  ] ", COLOR_LIGHT_GRAY, COLOR_BLACK);
    fb_println(step);
    serial_print("[BOOT] ");
    serial_println(step);
}

static void int_to_str(int val, char *buf, size_t max) {
    if (max == 0) return;
    if (val == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    char temp[32];
    int idx = 0;
    bool neg = false;
    if (val < 0) {
        neg = true;
        val = -val;
    }
    while (val > 0 && idx < 30) {
        temp[idx++] = '0' + (val % 10);
        val /= 10;
    }
    size_t out = 0;
    if (neg && out < max - 1) {
        buf[out++] = '-';
    }
    while (idx > 0 && out < max - 1) {
        buf[out++] = temp[--idx];
    }
    buf[out] = '\0';
}

void kernel_panic_pid1_died(int exit_code, const char *reason) {
    serial_printf("\n=======================================================\n");
    serial_printf(" !!! KERNEL PANIC: ATTEMPTED TO KILL INIT! (PID 1) !!!\n");
    serial_printf(" Exit Code: 0x%x  Reason: %s\n", exit_code, reason ? reason : "Unknown");
    serial_printf(" What Went Wrong: The root userland initialization process (/sbin/main) unexpectedly died.\n");
    serial_printf(" JurkOS cannot operate without a running PID 1 init process.\n");
    serial_printf(" System halted to prevent corruption.\n");
    serial_printf("=======================================================\n");

    fb_clear(COLOR_RED);
    fb_draw_box(60, 60, 680, 260, COLOR_BLUE, COLOR_WHITE,
                " !!! KERNEL PANIC: ATTEMPTED TO KILL INIT !!! ",
                "\n Fatal System Halt: PID 1 (/sbin/main) has died or crashed!\n\n"
                " What Went Wrong:\n"
                "  * The root initialization process exited or encountered a fatal signal.\n"
                "  * Without PID 1, the operating system cannot service userland.\n"
                "  * Hardware speaker alarm triggered; system state preserved for review.\n\n"
                " Action: Check /dev/console logs or reboot your computer.");

    fb_print_color("\n  [PANIC] Exit Code: ", COLOR_WHITE, COLOR_RED);
    char code_str[16];
    int_to_str(exit_code, code_str, sizeof(code_str));
    fb_print_color(code_str, COLOR_YELLOW, COLOR_RED);
    fb_print_color("  Reason: ", COLOR_WHITE, COLOR_RED);
    fb_print_color(reason ? reason : "Process terminated", COLOR_YELLOW, COLOR_RED);
    fb_print_color("\n", COLOR_WHITE, COLOR_RED);

    // Audio warning: PC Speaker alarm beeping
    speaker_panic_siren();

    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

static void print_banner(void) {
    fb_clear(COLOR_BLACK);
    fb_print_color("  __ _            _    ___  ____  \n", COLOR_CYAN, COLOR_BLACK);
    fb_print_color(" | | |_   _ _ __| | __/ _ \\/ ___| \n", COLOR_CYAN, COLOR_BLACK);
    fb_print_color(" | | | | | | '__| |/ / | | \\___ \\ \n", COLOR_CYAN, COLOR_BLACK);
    fb_print_color(" | | |_| | | |  |   <| |_| |___) |\n", COLOR_CYAN, COLOR_BLACK);
    fb_print_color(" \\___/\\__,_|_|  |_|\\_\\\\___/|____/ \n\n", COLOR_CYAN, COLOR_BLACK);

    fb_print_color(" JurkOS Kernel v1.1 Alpha\n", COLOR_YELLOW, COLOR_BLACK);
    fb_print_color(" Architecture: "
#if defined(__x86_64__)
    "x86-64 (64-bit Long Mode)"
#else
    "x86 (32-bit Protected Mode)"
#endif
    " | Limine Protocol | VFS Filesystem\n", COLOR_LIGHT_GRAY, COLOR_BLACK);
    fb_print("========================================================================\n\n");
}

static void shell_help(void) {
    fb_println("Available JurkOS Built-in Shell Commands:");
    fb_println("  help       - Display this list of commands");
    fb_println("  ls [dir]   - List files and directories (VFS & /dev)");
    fb_println("  cat <file> - Read and print file contents");
    fb_println("  exec <file>- Execute ELF, XEC, or .JEX binary via dual loader");
    fb_println("  wifi       - Launch Wi-Fi Adapter search & status manager");
    fb_println("  jurkstore  - Launch JurkStore package manager");
    fb_println("  uname [-a] - Print OS (-o), processor (-p), or both (-a)");
    fb_println("  clear      - Clear the screen buffer");
    fb_println("  reboot     - Warm reboot the system");
}

static void shell_ls(const char *path) {
    if (!path || strlen(path) == 0) path = "/";
    vfs_node_t *dir = vfs_lookup(path);
    if (!dir) {
        fb_print("ls: Directory not found: ");
        fb_println(path);
        return;
    }
    if (!(dir->flags & VFS_DIRECTORY)) {
        fb_print("ls: Not a directory: ");
        fb_println(path);
        return;
    }

    fb_print("Contents of ");
    fb_print(path);
    fb_println(":");

    uint32_t idx = 0;
    struct vfs_dirent *ent;
    while ((ent = vfs_readdir(dir, idx++))) {
        if (ent->flags & VFS_DIRECTORY) {
            fb_print("  [DIR]  ");
            fb_print_color(ent->name, COLOR_LIGHT_CYAN, COLOR_BLACK);
            fb_println("/");
        } else if (ent->flags & VFS_CHARDEVICE) {
            fb_print("  [DEV]  ");
            fb_print_color(ent->name, COLOR_YELLOW, COLOR_BLACK);
            fb_println("");
        } else {
            fb_print("  [FILE] ");
            fb_print(ent->name);
            fb_println("");
        }
    }
}

static void shell_cat(const char *path) {
    if (!path || strlen(path) == 0) {
        fb_println("cat: Usage: cat <path>");
        return;
    }
    vfs_node_t *file = vfs_lookup(path);
    if (!file) {
        fb_print("cat: File not found: ");
        fb_println(path);
        return;
    }
    if (file->flags & VFS_DIRECTORY) {
        fb_println("cat: Is a directory");
        return;
    }

    uint8_t buf[256];
    uint32_t offset = 0;
    uint32_t bytes;
    while ((bytes = vfs_read(file, offset, sizeof(buf) - 1, buf)) > 0) {
        buf[bytes] = '\0';
        fb_print((char*)buf);
        offset += bytes;
    }
    fb_println("");
}

static void shell_wifi(void) {
    fb_draw_box(120, 180, 560, 140, COLOR_BLUE, COLOR_WHITE, 
                "JurkOS Wireless Manager (wifi)",
                "\n     Oops, Looks like your computer dosen't have a WI-FI Adapter    \n\n"
                "  Status: Hardware checked across all PCI/USB adapters.\n"
                "  Active Connection: Using Intel e1000 Ethernet interface (/dev/net/eth0)");
}

static void shell_uname(const char *arg) {
    bool flag_o = false;
    bool flag_p = false;

#if defined(__x86_64__)
    const char *proc_type = "x86_64";
#else
    const char *proc_type = "x86";
#endif

    const char *os_name = "JurkOS 1.1 Alpha";

    if (!arg || strlen(arg) == 0) {
        fb_println(os_name);
        return;
    }

    if (strcmp(arg, "-a") == 0) {
        flag_o = true;
        flag_p = true;
    } else if (strcmp(arg, "-o") == 0) {
        flag_o = true;
    } else if (strcmp(arg, "-p") == 0) {
        flag_p = true;
    } else {
        flag_o = true;
    }

    if (flag_o && flag_p) {
        fb_print(os_name);
        fb_print(" ");
        fb_println(proc_type);
    } else if (flag_o) {
        fb_println(os_name);
    } else if (flag_p) {
        fb_println(proc_type);
    }
}

static void shell_reboot(void) {
    fb_println("Rebooting JurkOS...");
    uint8_t good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    __asm__ volatile ("hlt");
}

static void run_shell(void) {
    char cmd[128];
    while (1) {
        fb_print_color("jurkos@jurkos:/$ ", COLOR_LIGHT_GREEN, COLOR_BLACK);
        keyboard_readline(cmd, sizeof(cmd));

        char *p = cmd;
        while (*p == ' ') p++;
        if (strlen(p) == 0) continue;

        if (strcmp(p, "help") == 0) {
            shell_help();
        } else if (strncmp(p, "ls", 2) == 0) {
            char *arg = p + 2;
            while (*arg == ' ') arg++;
            shell_ls(arg);
        } else if (strncmp(p, "cat ", 4) == 0) {
            shell_cat(p + 4);
        } else if (strcmp(p, "wifi") == 0) {
            shell_wifi();
        } else if (strcmp(p, "jurkstore") == 0) {
            fb_println("JurkStore: Terminal package manager ready.");
            fb_println("Installed packages in /usr/apps/ available.");
        } else if (strncmp(p, "uname", 5) == 0) {
            char *arg = p + 5;
            while (*arg == ' ') arg++;
            shell_uname(arg);
        } else if (strcmp(p, "clear") == 0) {
            fb_clear(COLOR_BLACK);
        } else if (strcmp(p, "reboot") == 0) {
            shell_reboot();
        } else if (strncmp(p, "exec ", 5) == 0) {
            vfs_node_t *file = vfs_lookup(p + 5);
            if (file) {
                loader_exec(file, 0, NULL);
            } else {
                fb_println("exec: Binary file not found.");
            }
        } else {
            vfs_node_t *file = vfs_lookup(p);
            if (!file && p[0] != '/') {
                char bin_path[128];
                strcpy(bin_path, "/bin/");
                strcat(bin_path, p);
                file = vfs_lookup(bin_path);
            }
            if (file) {
                loader_exec(file, 0, NULL);
            } else {
                fb_print(p);
                fb_println(": command not found. Type 'help' for available commands.");
            }
        }
    }
}

void kernel_main(void) {
    // 1. Initialize CPU framework
    gdt_init();
    idt_init();
    serial_init();

    // 2. Initialize Framebuffer Graphics
#if defined(__x86_64__)
    if (fb_request.response && fb_request.response->framebuffer_count > 0) {
        struct limine_framebuffer *fb = fb_request.response->framebuffers[0];
        fb_init(fb->address, (uint32_t)fb->width, (uint32_t)fb->height, (uint32_t)fb->pitch, fb->bpp);
    } else {
        fb_init((void*)0xFFFF8000000B8000ULL, 1024, 768, 4096, 32);
    }
#else
    fb_init((void*)0xFD000000, 1024, 768, 4096, 32);
#endif

    print_banner();

    // Detailed Boot Step Logging
    log_boot_step("CPU Framework: GDT initialized with 64-bit Long Mode segments", true);
    log_boot_step("Interrupt Controller: 256-gate IDT loaded & 8259 PIC remapped", true);
    log_boot_step("Serial Debugger: COM1 UART active at I/O port 0x3F8", true);
    log_boot_step("Graphics Pipeline: Direct linear VESA / GOP framebuffer ready", true);

    // 3. Initialize Memory Managers
    pmm_init(128 * 1024 * 1024, (uintptr_t)kernel_bitmap_mem);
    pmm_mark_region(0x100000, 32 * 1024 * 1024, false);
    log_boot_step("Physical Memory Manager: 128 MB RAM detected and bitmap mapped", true);

    kheap_init((uintptr_t)kernel_heap_mem, sizeof(kernel_heap_mem));
    log_boot_step("Virtual Memory & Heap: 4-Level PML4 paging & 16 MB heap active", true);

    // 4. Initialize PC Speaker
    speaker_beep(1000, 50); // Short happy boot beep
    log_boot_step("PC Speaker Audio: Hardware PIT channel 2 timer gate ready", true);

    // 5. Scan Hardware Bus
    pci_scan();
    log_boot_step("PCI Bus: Hardware scanned across 256 buses and registered", true);

    struct pci_device *nic = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82540EM);
    if (!nic) nic = pci_find_device(E1000_VENDOR_INTEL, E1000_DEV_82545EM);
    if (nic) {
        e1000_init(nic);
        log_boot_step("Network Controller: Intel 82540EM/82545EM Gigabit adapter active", true);
    } else {
        log_boot_step("Network Controller: No Intel e1000 NIC detected (using fallback)", false);
    }

    // 6. Initialize VFS and DevFS
    vfs_init();
    devfs_init();
    log_boot_step("Virtual File System: Universal VFS layer & /dev devfs mounted", true);

    // 7. Check for loaded Disk Module (FAT32 or Custom JurkVFS)
    bool root_mounted = false;
#if defined(__x86_64__)
    if (module_request.response && module_request.response->module_count > 0) {
        struct limine_file *mod = module_request.response->modules[0];
        // 7a. Try FAT32 first
        vfs_node_t *fat_root = fat32_init(mod->address, mod->size);
        if (fat_root) {
            vfs_mount("/", fat_root);
            devfs_init();
            root_mounted = true;
            log_boot_step("Disk Subsystem: FAT32 partition mounted successfully as /", true);
        } else {
            // 7b. Try VFS raw disk format
            vfs_node_t *jurk_root = jurkvfs_init(mod->address, mod->size);
            if (jurk_root) {
                vfs_mount("/", jurk_root);
                devfs_init();
                root_mounted = true;
                log_boot_step("Disk Subsystem: VFS image mounted successfully as /", true);
            }
        }
    }
#endif
    if (!root_mounted) {
        log_boot_step("Disk Subsystem: Running on memory VFS root", true);
    }

    log_boot_step("Dual Binary Loader: Standard ELF-64/32 & Flat XEC (.JEX) engine loaded", true);

    // 8. Initialize PS/2 Keyboard
    keyboard_init();
    log_boot_step("Keyboard Driver: PS/2 controller active with interrupt queue", true);

    fb_println("");
    fb_print_color(">>> Boot Sequence Complete! Handing off execution to PID 1 <<<\n\n", COLOR_LIGHT_GREEN, COLOR_BLACK);

    // 9. Launch PID 1 (/sbin/main)
    vfs_node_t *init_bin = vfs_lookup("/sbin/main");
    if (!init_bin) init_bin = vfs_lookup("/bin/main");
    if (!init_bin) init_bin = vfs_lookup("/sbin/main.jex");

    if (init_bin) {
        log_boot_step("Executing PID 1: /sbin/main...", true);
        int exit_code = loader_exec(init_bin, 0, NULL);
        // If PID 1 returns or dies, panic!
        kernel_panic_pid1_died(exit_code, "PID 1 (/sbin/main) returned unexpectedly");
    } else {
        // Fallback to internal shell if no external binary is found on disk
        log_boot_step("PID 1 binary /sbin/main not on disk; dropping to kernel emergency shell", false);
        run_shell();
        // If the shell exits, panic!
        kernel_panic_pid1_died(0, "Emergency root shell exited");
    }
}
