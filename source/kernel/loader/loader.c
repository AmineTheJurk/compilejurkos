#include "../include/loader.h"
#include "../include/kheap.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/serial.h"
#include "../include/framebuffer.h"

extern int elf64_load(const uint8_t *data, size_t size, uintptr_t *entry_out);
extern int elf32_load(const uint8_t *data, size_t size, uintptr_t *entry_out);
extern int xec_load(const uint8_t *data, size_t size, uintptr_t *entry_out);

binary_type_t loader_probe(const uint8_t *buffer, size_t size) {
    if (!buffer || size < 4) return BIN_UNKNOWN;

    if (buffer[0] == 0x7F && buffer[1] == 'E' && buffer[2] == 'L' && buffer[3] == 'F') {
        if (size > 4 && buffer[4] == 2) return BIN_ELF64;
        if (size > 4 && buffer[4] == 1) return BIN_ELF32;
    }

    return BIN_XEC_FLAT;
}

int loader_exec_buffer(const uint8_t *buffer, size_t size, const char *name, int argc, char **argv) {
    (void)argc; (void)argv;
    uintptr_t entry = 0;
    binary_type_t type = loader_probe(buffer, size);

    if (type == BIN_ELF64) {
        serial_printf("Loader: Executing '%s' as ELF-64 binary...\n", name);
        if (elf64_load(buffer, size, &entry) != 0) {
            serial_printf("Loader: Failed to load ELF-64 binary!\n");
            return -1;
        }
    } else if (type == BIN_ELF32) {
        serial_printf("Loader: Executing '%s' as ELF-32 binary...\n", name);
        if (elf32_load(buffer, size, &entry) != 0) {
            serial_printf("Loader: Failed to load ELF-32 binary!\n");
            return -1;
        }
    } else {
        serial_printf("Loader: Executing '%s' as XEC flat binary...\n", name);
        if (xec_load(buffer, size, &entry) != 0) {
            serial_printf("Loader: Failed to load XEC flat binary!\n");
            return -1;
        }
    }

    // Allocate 64KB user stack
    uintptr_t stack_top;
#if defined(__x86_64__)
    uintptr_t stack_base = 0x00007FFFF0000000ULL;
    stack_top = stack_base + 0x10000;
#else
    uintptr_t stack_base = 0x70000000;
    stack_top = stack_base + 0x10000;
#endif

    for (int p = 0; p < 16; p++) {
        void *page = pmm_alloc_page();
        vmm_map_page((uintptr_t)page, stack_base + (p * PAGE_SIZE), PAGE_USER | PAGE_WRITABLE);
    }

    serial_printf("Loader: Jumping to user entry point 0x%x (Stack: 0x%x)\n", entry, stack_top);

    // Call entry point
    typedef int (*entry_func_t)(int, char**);
    entry_func_t app_entry = (entry_func_t)entry;
    int ret = app_entry(argc, argv);
    serial_printf("Loader: Application '%s' exited with code %d\n", name, ret);
    return ret;
}

int loader_exec(vfs_node_t *file, int argc, char **argv) {
    if (!file || file->length == 0) {
        serial_printf("Loader: Cannot execute empty file!\n");
        return -1;
    }

    uint8_t *buffer = (uint8_t*)kmalloc(file->length);
    if (!buffer) {
        serial_printf("Loader: Out of memory reading binary!\n");
        return -1;
    }

    uint32_t bytes_read = vfs_read(file, 0, file->length, buffer);
    if (bytes_read == 0) {
        kfree(buffer);
        return -1;
    }

    int ret = loader_exec_buffer(buffer, bytes_read, file->name, argc, argv);
    kfree(buffer);
    return ret;
}
