#ifndef JURKOS_JURKVFS_H
#define JURKOS_JURKVFS_H

#include "types.h"
#include "vfs.h"

#define JURKVFS_MAGIC "JURKVFS1"
#define JURKVFS_MAGIC_LEN 8

#define JURKVFS_TYPE_FILE 1
#define JURKVFS_TYPE_DIR  2

struct jurkvfs_header {
    char magic[JURKVFS_MAGIC_LEN];
    uint32_t version;
    uint32_t block_size;
    uint32_t total_inodes;
    uint32_t inode_table_offset;
    uint32_t data_offset;
    uint32_t total_size;
    uint8_t  reserved[32];
} __attribute__((packed));

struct jurkvfs_inode {
    char name[64];
    uint32_t type;
    uint32_t size;
    uint32_t offset;
    uint32_t parent_inode;
    uint32_t permissions;
    uint8_t  reserved[8];
} __attribute__((packed));

vfs_node_t *jurkvfs_init(void *disk_image, size_t image_size);

#endif
