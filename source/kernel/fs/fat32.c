#include "../include/fat32.h"
#include "../include/string.h"
#include "../include/kheap.h"
#include "../include/serial.h"

struct fat32_fs {
    uint8_t  *disk_base;
    size_t    disk_size;
    uint32_t  partition_offset;
    uint32_t  bytes_per_sector;
    uint32_t  sectors_per_cluster;
    uint32_t  cluster_size;
    uint32_t  reserved_sectors;
    uint32_t  fat_start_sector;
    uint32_t  fat_size_sectors;
    uint32_t  data_start_sector;
    uint32_t  root_cluster;
};

static struct fat32_fs current_fat;
static struct vfs_dirent fat_global_dirent;

struct fat32_node_priv {
    uint32_t first_cluster;
    uint32_t file_size;
    struct fat32_fs *fs;
};

static uint32_t fat32_get_next_cluster(struct fat32_fs *fs, uint32_t cluster) {
    uint32_t fat_offset = cluster * 4;
    uint32_t fat_sector = fs->fat_start_sector + (fat_offset / fs->bytes_per_sector);
    uint32_t ent_offset = fat_offset % fs->bytes_per_sector;

    uint8_t *sector_ptr = fs->disk_base + (fat_sector * fs->bytes_per_sector);
    uint32_t next = *(uint32_t*)(sector_ptr + ent_offset) & 0x0FFFFFFF;
    return next;
}

static uint8_t *fat32_get_cluster_ptr(struct fat32_fs *fs, uint32_t cluster) {
    if (cluster < 2) return NULL;
    uint32_t sector = fs->data_start_sector + ((cluster - 2) * fs->sectors_per_cluster);
    return fs->disk_base + (sector * fs->bytes_per_sector);
}

// Format 8.3 FAT filename to clean null-terminated lowercase string
static void fat32_format_name(const char *raw, char *out) {
    int out_idx = 0;
    // Base name (up to 8 chars)
    for (int i = 0; i < 8; i++) {
        char c = raw[i];
        if (c == ' ') break;
        if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
        out[out_idx++] = c;
    }
    // Extension (up to 3 chars)
    if (raw[8] != ' ') {
        out[out_idx++] = '.';
        for (int i = 8; i < 11; i++) {
            char c = raw[i];
            if (c == ' ') break;
            if (c >= 'A' && c <= 'Z') c += ('a' - 'A');
            out[out_idx++] = c;
        }
    }
    out[out_idx] = '\0';
}

static uint32_t fat32_read_impl(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    struct fat32_node_priv *priv = (struct fat32_node_priv*)node->device_priv;
    if (!priv || !priv->fs) return 0;
    if (offset >= priv->file_size) return 0;
    if (offset + size > priv->file_size) {
        size = priv->file_size - offset;
    }

    struct fat32_fs *fs = priv->fs;
    uint32_t cluster = priv->first_cluster;
    uint32_t cluster_size = fs->cluster_size;

    // Advance to starting cluster
    uint32_t skip_clusters = offset / cluster_size;
    uint32_t cluster_offset = offset % cluster_size;

    while (skip_clusters > 0 && cluster < 0x0FFFFFF8) {
        cluster = fat32_get_next_cluster(fs, cluster);
        skip_clusters--;
    }

    uint32_t bytes_read = 0;
    while (bytes_read < size && cluster < 0x0FFFFFF8) {
        uint8_t *cptr = fat32_get_cluster_ptr(fs, cluster);
        if (!cptr) break;

        uint32_t to_copy = cluster_size - cluster_offset;
        if (to_copy > (size - bytes_read)) {
            to_copy = size - bytes_read;
        }

        memcpy(buffer + bytes_read, cptr + cluster_offset, to_copy);
        bytes_read += to_copy;
        cluster_offset = 0; // Only applies to first cluster

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    return bytes_read;
}

static vfs_node_t *fat32_create_node(struct fat32_fs *fs, const char *name, uint32_t cluster, uint32_t size, bool is_dir);

static struct vfs_dirent *fat32_readdir_impl(vfs_node_t *node, uint32_t index) {
    struct fat32_node_priv *priv = (struct fat32_node_priv*)node->device_priv;
    if (!priv || !priv->fs) return NULL;

    struct fat32_fs *fs = priv->fs;
    uint32_t cluster = priv->first_cluster;
    uint32_t current_match = 0;

    while (cluster < 0x0FFFFFF8) {
        uint8_t *cptr = fat32_get_cluster_ptr(fs, cluster);
        if (!cptr) break;

        struct fat32_dirent *entries = (struct fat32_dirent*)cptr;
        uint32_t max_entries = fs->cluster_size / sizeof(struct fat32_dirent);

        for (uint32_t i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) return NULL; // End of dir
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;   // Deleted
            if (entries[i].attr == FAT_ATTR_LFN) continue;       // Skip LFN raw entries
            if (entries[i].attr & FAT_ATTR_VOLUME_ID) continue;  // Skip volume label

            // Skip '.' and '..'
            if (entries[i].name[0] == '.') continue;

            if (current_match == index) {
                fat32_format_name(entries[i].name, fat_global_dirent.name);
                fat_global_dirent.inode = ((uint32_t)entries[i].first_cluster_hi << 16) | entries[i].first_cluster_lo;
                fat_global_dirent.flags = (entries[i].attr & FAT_ATTR_DIRECTORY) ? VFS_DIRECTORY : VFS_FILE;
                return &fat_global_dirent;
            }
            current_match++;
        }

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    return NULL;
}

static vfs_node_t *fat32_finddir_impl(vfs_node_t *node, const char *name) {
    struct fat32_node_priv *priv = (struct fat32_node_priv*)node->device_priv;
    if (!priv || !priv->fs || !name) return NULL;

    struct fat32_fs *fs = priv->fs;
    uint32_t cluster = priv->first_cluster;

    char formatted[64];

    while (cluster < 0x0FFFFFF8) {
        uint8_t *cptr = fat32_get_cluster_ptr(fs, cluster);
        if (!cptr) break;

        struct fat32_dirent *entries = (struct fat32_dirent*)cptr;
        uint32_t max_entries = fs->cluster_size / sizeof(struct fat32_dirent);

        for (uint32_t i = 0; i < max_entries; i++) {
            if ((uint8_t)entries[i].name[0] == 0x00) return NULL;
            if ((uint8_t)entries[i].name[0] == 0xE5) continue;
            if (entries[i].attr == FAT_ATTR_LFN) continue;
            if (entries[i].attr & FAT_ATTR_VOLUME_ID) continue;

            fat32_format_name(entries[i].name, formatted);
            if (strcmp(formatted, name) == 0) {
                uint32_t first_cl = ((uint32_t)entries[i].first_cluster_hi << 16) | entries[i].first_cluster_lo;
                bool is_dir = (entries[i].attr & FAT_ATTR_DIRECTORY) != 0;
                return fat32_create_node(fs, formatted, first_cl, entries[i].file_size, is_dir);
            }
        }

        cluster = fat32_get_next_cluster(fs, cluster);
    }

    return NULL;
}

static vfs_node_t *fat32_create_node(struct fat32_fs *fs, const char *name, uint32_t cluster, uint32_t size, bool is_dir) {
    vfs_node_t *node = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    strcpy(node->name, name);
    node->inode = cluster;
    node->length = size;

    struct fat32_node_priv *priv = (struct fat32_node_priv*)kmalloc(sizeof(struct fat32_node_priv));
    priv->first_cluster = cluster;
    priv->file_size = size;
    priv->fs = fs;
    node->device_priv = priv;

    if (is_dir) {
        node->flags = VFS_DIRECTORY;
        node->readdir = fat32_readdir_impl;
        node->finddir = fat32_finddir_impl;
    } else {
        node->flags = VFS_FILE;
        node->read = fat32_read_impl;
    }
    return node;
}

vfs_node_t *fat32_init(void *disk_data, size_t disk_size) {
    if (!disk_data || disk_size < 512) return NULL;

    uint8_t *raw = (uint8_t*)disk_data;
    uint32_t partition_lba = 0;

    // Check if this is an MBR partition table (offset 446 has partition 1)
    if (raw[510] == 0x55 && raw[511] == 0xAA) {
        uint8_t part_type = raw[446 + 4];
        // 0x0B or 0x0C is FAT32
        if (part_type == 0x0B || part_type == 0x0C) {
            partition_lba = *(uint32_t*)(&raw[446 + 8]);
            serial_printf("FAT32: Found MBR FAT32 partition at LBA %d\n", partition_lba);
        }
    }

    struct fat32_bpb *bpb = (struct fat32_bpb*)(raw + (partition_lba * 512));

    // Verify FAT32 signature
    if (bpb->bytes_per_sector == 0 || bpb->sectors_per_cluster == 0) {
        return NULL;
    }
    if (memcmp(bpb->fs_type, "FAT32   ", 8) != 0 && bpb->root_cluster == 0) {
        return NULL;
    }

    current_fat.disk_base = raw;
    current_fat.disk_size = disk_size;
    current_fat.partition_offset = partition_lba * 512;
    current_fat.bytes_per_sector = bpb->bytes_per_sector;
    current_fat.sectors_per_cluster = bpb->sectors_per_cluster;
    current_fat.cluster_size = bpb->bytes_per_sector * bpb->sectors_per_cluster;
    current_fat.reserved_sectors = bpb->reserved_sector_count;
    current_fat.fat_start_sector = partition_lba + bpb->reserved_sector_count;
    current_fat.fat_size_sectors = bpb->table_size_32;
    current_fat.data_start_sector = current_fat.fat_start_sector + (bpb->num_fats * bpb->table_size_32);
    current_fat.root_cluster = bpb->root_cluster;

    serial_printf("FAT32: Initialized successfully! (Cluster size: %d bytes, Root cluster: %d)\n",
                  current_fat.cluster_size, current_fat.root_cluster);

    return fat32_create_node(&current_fat, "/", current_fat.root_cluster, 0, true);
}
