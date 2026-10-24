#include "../include/devfs.h"
#include "../include/string.h"
#include "../include/kheap.h"
#include "../include/framebuffer.h"
#include "../include/serial.h"
#include "../include/keyboard.h"
#include "../include/e1000.h"
#include "../include/pci.h"

#define MAX_DEVFS_ENTRIES 64

struct devfs_entry {
    vfs_node_t node;
    struct devfs_entry *children[16];
    uint32_t child_count;
};

static struct devfs_entry *dev_root = NULL;

static struct vfs_dirent global_dirent;

static struct vfs_dirent *devfs_readdir_impl(vfs_node_t *node, uint32_t index) {
    struct devfs_entry *entry = (struct devfs_entry*)node;
    if (index >= entry->child_count) return NULL;

    struct devfs_entry *child = entry->children[index];
    strcpy(global_dirent.name, child->node.name);
    global_dirent.inode = child->node.inode;
    global_dirent.flags = child->node.flags;
    return &global_dirent;
}

static vfs_node_t *devfs_finddir_impl(vfs_node_t *node, const char *name) {
    struct devfs_entry *entry = (struct devfs_entry*)node;
    for (uint32_t i = 0; i < entry->child_count; i++) {
        if (strcmp(entry->children[i]->node.name, name) == 0) {
            return &entry->children[i]->node;
        }
    }
    return NULL;
}

// Built-in device read/write handlers
static uint32_t dev_null_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    return size;
}

static uint32_t dev_zero_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    memset(buffer, 0, size);
    return size;
}

static uint32_t dev_urandom_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    static uint32_t seed = 0x12345678;
    for (uint32_t i = 0; i < size; i++) {
        seed = seed * 1103515245 + 12345;
        buffer[i] = (uint8_t)(seed >> 16);
    }
    return size;
}

static uint32_t dev_console_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        char s[2] = {(char)buffer[i], '\0'};
        fb_print(s);
        serial_putc((char)buffer[i]);
    }
    return size;
}

static uint32_t dev_kbd_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)keyboard_getchar();
    }
    return size;
}

static uint32_t dev_fb_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node;
    uint8_t *fb = (uint8_t*)fb_get_buffer();
    if (!fb) return 0;
    memcpy(fb + offset, buffer, size);
    return size;
}

static uint32_t dev_eth_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    (void)node; (void)offset;
    return (uint32_t)e1000_send_packet(buffer, (uint16_t)size);
}

static uint32_t dev_eth_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    return (uint32_t)e1000_receive_packet(buffer, (uint16_t)size);
}

static struct devfs_entry *devfs_create_entry(const char *name, uint32_t flags) {
    struct devfs_entry *entry = (struct devfs_entry*)kzalloc(sizeof(struct devfs_entry));
    strcpy(entry->node.name, name);
    entry->node.flags = flags;
    entry->node.readdir = devfs_readdir_impl;
    entry->node.finddir = devfs_finddir_impl;
    return entry;
}

void devfs_register_folder(const char *folder_path) {
    if (!dev_root || !folder_path) return;
    struct devfs_entry *sub = devfs_create_entry(folder_path, VFS_DIRECTORY);
    if (dev_root->child_count < 16) {
        dev_root->children[dev_root->child_count++] = sub;
    }
}

vfs_node_t *devfs_register(const char *path, uint32_t flags, read_type_t read, write_type_t write) {
    if (!dev_root || !path) return NULL;

    // Check if path is in subfolder, e.g. "net/eth0"
    const char *slash = NULL;
    for (const char *p = path; *p; p++) {
        if (*p == '/') { slash = p; break; }
    }

    struct devfs_entry *parent = dev_root;
    char node_name[64];

    if (slash) {
        char folder[32];
        size_t flen = slash - path;
        strncpy(folder, path, flen);
        folder[flen] = '\0';
        strcpy(node_name, slash + 1);

        // Find or create folder in dev_root
        struct devfs_entry *found_folder = NULL;
        for (uint32_t i = 0; i < dev_root->child_count; i++) {
            if (strcmp(dev_root->children[i]->node.name, folder) == 0) {
                found_folder = dev_root->children[i];
                break;
            }
        }
        if (!found_folder) {
            found_folder = devfs_create_entry(folder, VFS_DIRECTORY);
            if (dev_root->child_count < 16) {
                dev_root->children[dev_root->child_count++] = found_folder;
            }
        }
        parent = found_folder;
    } else {
        strcpy(node_name, path);
    }

    struct devfs_entry *entry = devfs_create_entry(node_name, flags);
    entry->node.read = read;
    entry->node.write = write;

    if (parent->child_count < 16) {
        parent->children[parent->child_count++] = entry;
    }
    return &entry->node;
}

void devfs_init(void) {
    dev_root = devfs_create_entry("dev", VFS_DIRECTORY);

    // Register standard character devices
    devfs_register("null", VFS_CHARDEVICE, NULL, dev_null_write);
    devfs_register("zero", VFS_CHARDEVICE, dev_zero_read, NULL);
    devfs_register("urandom", VFS_CHARDEVICE, dev_urandom_read, NULL);
    devfs_register("console", VFS_CHARDEVICE, NULL, dev_console_write);
    devfs_register("tty", VFS_CHARDEVICE, dev_kbd_read, dev_console_write);
    devfs_register("ttyS0", VFS_CHARDEVICE, NULL, dev_console_write);
    devfs_register("fb0", VFS_CHARDEVICE, NULL, dev_fb_write);

    // Subfolders
    devfs_register("input/kbd0", VFS_CHARDEVICE, dev_kbd_read, NULL);
    devfs_register("net/eth0", VFS_CHARDEVICE, dev_eth_read, dev_eth_write);
    devfs_register("disk/jurk_root", VFS_BLOCKDEVICE, NULL, NULL);
    devfs_register("disk/sda", VFS_BLOCKDEVICE, NULL, NULL);

    // Enumerate PCI devices into /dev/pci/
    uint32_t pci_count = pci_get_device_count();
    for (uint32_t i = 0; i < pci_count; i++) {
        struct pci_device *dev = pci_get_device(i);
        if (dev) {
            char pci_node[32];
            // e.g. "pci/00:03.0"
            pci_node[0] = 'p'; pci_node[1] = 'c'; pci_node[2] = 'i'; pci_node[3] = '/';
            pci_node[4] = '0' + (dev->bus / 10);
            pci_node[5] = '0' + (dev->bus % 10);
            pci_node[6] = ':';
            pci_node[7] = '0' + (dev->slot / 10);
            pci_node[8] = '0' + (dev->slot % 10);
            pci_node[9] = '.';
            pci_node[10] = '0' + dev->func;
            pci_node[11] = '\0';
            devfs_register(pci_node, VFS_CHARDEVICE, NULL, NULL);
        }
    }

    // Mount /dev onto root VFS
    vfs_mount("/dev", &dev_root->node);
    serial_printf("DevFS: Successfully populated and mounted /dev with device tree.\n");
}
