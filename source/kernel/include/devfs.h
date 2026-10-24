#ifndef JURKOS_DEVFS_H
#define JURKOS_DEVFS_H

#include "vfs.h"

void devfs_init(void);
vfs_node_t *devfs_register(const char *path, uint32_t flags, read_type_t read, write_type_t write);
void devfs_register_folder(const char *folder_path);

#endif
