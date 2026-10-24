#ifndef JURKOS_VFS_H
#define JURKOS_VFS_H

#include "types.h"

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_CHARDEVICE  0x03
#define VFS_BLOCKDEVICE 0x04

#define FS_MAX_NAME 128

struct vfs_node;

typedef uint32_t (*read_type_t)(struct vfs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct vfs_node*, uint32_t, uint32_t, const uint8_t*);
typedef void (*open_type_t)(struct vfs_node*);
typedef void (*close_type_t)(struct vfs_node*);
typedef struct vfs_dirent * (*readdir_type_t)(struct vfs_node*, uint32_t);
typedef struct vfs_node * (*finddir_type_t)(struct vfs_node*, const char *name);

typedef struct vfs_node {
    char name[FS_MAX_NAME];
    uint32_t flags;
    uint32_t length;
    uint32_t inode;
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
    struct vfs_node *ptr; // Alias or mount pointer
    void *device_priv;    // Driver private data
} vfs_node_t;

struct vfs_dirent {
    char name[FS_MAX_NAME];
    uint32_t inode;
    uint32_t flags;
};

extern vfs_node_t *vfs_root;

void vfs_init(void);
uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer);
void vfs_open(vfs_node_t *node);
void vfs_close(vfs_node_t *node);
struct vfs_dirent *vfs_readdir(vfs_node_t *node, uint32_t index);
vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name);
vfs_node_t *vfs_lookup(const char *path);
int vfs_mount(const char *path, vfs_node_t *target);

#endif
