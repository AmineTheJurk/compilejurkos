#include "../include/jurkvfs.h"
#include "../include/string.h"
#include "../include/kheap.h"
#include "../include/serial.h"

struct jurkvfs_fs {
    void *disk_data;
    size_t disk_size;
    struct jurkvfs_header *header;
    struct jurkvfs_inode *inodes;
};

static struct jurkvfs_fs current_fs;
static struct vfs_dirent global_dirent;

struct jurkvfs_node_priv {
    uint32_t inode_index;
    struct jurkvfs_fs *fs;
};

static uint32_t jurkvfs_read_impl(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    struct jurkvfs_node_priv *priv = (struct jurkvfs_node_priv*)node->device_priv;
    if (!priv || !priv->fs) return 0;

    struct jurkvfs_inode *inode = &priv->fs->inodes[priv->inode_index];
    if (offset >= inode->size) return 0;
    if (offset + size > inode->size) {
        size = inode->size - offset;
    }

    uint8_t *data_base = (uint8_t*)priv->fs->disk_data + priv->fs->header->data_offset;
    memcpy(buffer, data_base + inode->offset + offset, size);
    return size;
}

static struct vfs_dirent *jurkvfs_readdir_impl(vfs_node_t *node, uint32_t index) {
    struct jurkvfs_node_priv *priv = (struct jurkvfs_node_priv*)node->device_priv;
    if (!priv || !priv->fs) return NULL;

    uint32_t current_match = 0;
    for (uint32_t i = 0; i < priv->fs->header->total_inodes; i++) {
        if (priv->fs->inodes[i].parent_inode == priv->inode_index && i != priv->inode_index) {
            if (current_match == index) {
                strcpy(global_dirent.name, priv->fs->inodes[i].name);
                global_dirent.inode = i;
                global_dirent.flags = (priv->fs->inodes[i].type == JURKVFS_TYPE_DIR) ? VFS_DIRECTORY : VFS_FILE;
                return &global_dirent;
            }
            current_match++;
        }
    }
    return NULL;
}

static vfs_node_t *jurkvfs_create_vfs_node(struct jurkvfs_fs *fs, uint32_t inode_idx);

static vfs_node_t *jurkvfs_finddir_impl(vfs_node_t *node, const char *name) {
    struct jurkvfs_node_priv *priv = (struct jurkvfs_node_priv*)node->device_priv;
    if (!priv || !priv->fs) return NULL;

    for (uint32_t i = 0; i < priv->fs->header->total_inodes; i++) {
        if (priv->fs->inodes[i].parent_inode == priv->inode_index && i != priv->inode_index) {
            if (strcmp(priv->fs->inodes[i].name, name) == 0) {
                return jurkvfs_create_vfs_node(priv->fs, i);
            }
        }
    }
    return NULL;
}

static vfs_node_t *jurkvfs_create_vfs_node(struct jurkvfs_fs *fs, uint32_t inode_idx) {
    struct jurkvfs_inode *inode = &fs->inodes[inode_idx];
    vfs_node_t *node = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    strcpy(node->name, inode->name);
    node->inode = inode_idx;
    node->length = inode->size;

    struct jurkvfs_node_priv *priv = (struct jurkvfs_node_priv*)kmalloc(sizeof(struct jurkvfs_node_priv));
    priv->inode_index = inode_idx;
    priv->fs = fs;
    node->device_priv = priv;

    if (inode->type == JURKVFS_TYPE_DIR) {
        node->flags = VFS_DIRECTORY;
        node->readdir = jurkvfs_readdir_impl;
        node->finddir = jurkvfs_finddir_impl;
    } else {
        node->flags = VFS_FILE;
        node->read = jurkvfs_read_impl;
    }
    return node;
}

vfs_node_t *jurkvfs_init(void *disk_image, size_t image_size) {
    if (!disk_image || image_size < sizeof(struct jurkvfs_header)) {
        return NULL;
    }

    struct jurkvfs_header *hdr = (struct jurkvfs_header*)disk_image;
    if (memcmp(hdr->magic, JURKVFS_MAGIC, JURKVFS_MAGIC_LEN) != 0) {
        serial_printf("JurkVFS: Invalid disk magic! Expected 'JURKVFS1'\n");
        return NULL;
    }

    current_fs.disk_data = disk_image;
    current_fs.disk_size = image_size;
    current_fs.header = hdr;
    current_fs.inodes = (struct jurkvfs_inode*)((uint8_t*)disk_image + hdr->inode_table_offset);

    serial_printf("JurkVFS: Mounted valid raw VFS disk image (Inodes: %d, Data offset: 0x%x)\n",
                  hdr->total_inodes, hdr->data_offset);

    // Root inode is always index 0
    return jurkvfs_create_vfs_node(&current_fs, 0);
}
