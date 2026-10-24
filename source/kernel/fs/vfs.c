#include "../include/vfs.h"
#include "../include/string.h"
#include "../include/kheap.h"
#include "../include/serial.h"

vfs_node_t *vfs_root = NULL;

void vfs_init(void) {
    vfs_root = (vfs_node_t*)kzalloc(sizeof(vfs_node_t));
    strcpy(vfs_root->name, "/");
    vfs_root->flags = VFS_DIRECTORY;
}

uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    if (!node || !buffer) return 0;
    if (node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, const uint8_t *buffer) {
    if (!node || !buffer) return 0;
    if (node->write) {
        return node->write(node, offset, size, buffer);
    }
    return 0;
}

void vfs_open(vfs_node_t *node) {
    if (node && node->open) {
        node->open(node);
    }
}

void vfs_close(vfs_node_t *node) {
    if (node && node->close) {
        node->close(node);
    }
}

struct vfs_dirent *vfs_readdir(vfs_node_t *node, uint32_t index) {
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->readdir) {
        return NULL;
    }
    return node->readdir(node, index);
}

vfs_node_t *vfs_finddir(vfs_node_t *node, const char *name) {
    if (!node || !(node->flags & VFS_DIRECTORY) || !node->finddir) {
        return NULL;
    }
    return node->finddir(node, name);
}

vfs_node_t *vfs_lookup(const char *path) {
    if (!path || !vfs_root) return NULL;
    if (strcmp(path, "/") == 0) return vfs_root;

    vfs_node_t *current = vfs_root;
    char token[FS_MAX_NAME];
    size_t i = 0;

    if (*path == '/') path++;

    while (*path) {
        i = 0;
        while (*path && *path != '/' && i < FS_MAX_NAME - 1) {
            token[i++] = *path++;
        }
        token[i] = '\0';
        if (*path == '/') path++;

        if (i == 0) continue;

        // Follow mount pointers if present
        if (current->ptr) {
            current = current->ptr;
        }

        vfs_node_t *next = vfs_finddir(current, token);
        if (!next) {
            return NULL;
        }
        current = next;
    }

    if (current->ptr) return current->ptr;
    return current;
}

int vfs_mount(const char *path, vfs_node_t *target) {
    if (!path || !target) return -1;
    if (strcmp(path, "/") == 0) {
        vfs_root = target;
        return 0;
    }

    vfs_node_t *mount_point = vfs_lookup(path);
    if (!mount_point) {
        serial_printf("VFS: Mount point '%s' not found!\n", path);
        return -1;
    }

    mount_point->ptr = target;
    serial_printf("VFS: Mounted target to '%s'\n", path);
    return 0;
}
