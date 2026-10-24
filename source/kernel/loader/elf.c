#include "../include/loader.h"
#include "../include/vmm.h"
#include "../include/pmm.h"
#include "../include/string.h"
#include "../include/kheap.h"
#include "../include/serial.h"

int elf64_load(const uint8_t *data, size_t size, uintptr_t *entry_out) {
    if (size < sizeof(Elf64_Ehdr)) return -1;
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr*)data;

    // Check ELF magic: 0x7F 'E' 'L' 'F'
    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F') {
        return -1;
    }

    // 2 = 64-bit ELF
    if (ehdr->e_ident[4] != 2) return -1;

    serial_printf("ELF64: Valid header found, Entry: 0x%x, Phdrs: %d\n", 
                  ehdr->e_entry, ehdr->e_phnum);

    const Elf64_Phdr *phdrs = (const Elf64_Phdr*)(data + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uintptr_t vaddr = phdrs[i].p_vaddr;
        size_t memsz    = phdrs[i].p_memsz;
        size_t filesz   = phdrs[i].p_filesz;
        uintptr_t offset= phdrs[i].p_offset;

        uint64_t page_flags = PAGE_USER;
        if (phdrs[i].p_flags & PF_W) page_flags |= PAGE_WRITABLE;

        uintptr_t start_page = vaddr & ~0xFFFULL;
        uintptr_t end_page   = (vaddr + memsz + 0xFFFULL) & ~0xFFFULL;
        size_t pages_needed  = (end_page - start_page) / PAGE_SIZE;

        for (size_t p = 0; p < pages_needed; p++) {
            uintptr_t virt = start_page + (p * PAGE_SIZE);
            if (!vmm_get_phys(virt)) {
                void *phys = pmm_alloc_page();
                memset(phys, 0, PAGE_SIZE);
                vmm_map_page((uintptr_t)phys, virt, page_flags);
            }
        }

        // Copy segment contents
        if (filesz > 0 && offset + filesz <= size) {
            memcpy((void*)vaddr, data + offset, filesz);
        }
        // Zero remaining BSS
        if (memsz > filesz) {
            memset((void*)(vaddr + filesz), 0, memsz - filesz);
        }
    }

    *entry_out = ehdr->e_entry;
    return 0;
}

int elf32_load(const uint8_t *data, size_t size, uintptr_t *entry_out) {
    if (size < sizeof(Elf32_Ehdr)) return -1;
    const Elf32_Ehdr *ehdr = (const Elf32_Ehdr*)data;

    if (ehdr->e_ident[0] != 0x7F || ehdr->e_ident[1] != 'E' ||
        ehdr->e_ident[2] != 'L'  || ehdr->e_ident[3] != 'F') {
        return -1;
    }

    // 1 = 32-bit ELF
    if (ehdr->e_ident[4] != 1) return -1;

    serial_printf("ELF32: Valid header found, Entry: 0x%x\n", ehdr->e_entry);
    const Elf32_Phdr *phdrs = (const Elf32_Phdr*)(data + ehdr->e_phoff);

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        if (phdrs[i].p_type != PT_LOAD) continue;

        uintptr_t vaddr = phdrs[i].p_vaddr;
        size_t memsz    = phdrs[i].p_memsz;
        size_t filesz   = phdrs[i].p_filesz;
        uintptr_t offset= phdrs[i].p_offset;

        uint64_t page_flags = PAGE_USER;
        if (phdrs[i].p_flags & PF_W) page_flags |= PAGE_WRITABLE;

        uintptr_t start_page = vaddr & ~0xFFF;
        uintptr_t end_page   = (vaddr + memsz + 0xFFF) & ~0xFFF;
        size_t pages_needed  = (end_page - start_page) / PAGE_SIZE;

        for (size_t p = 0; p < pages_needed; p++) {
            uintptr_t virt = start_page + (p * PAGE_SIZE);
            if (!vmm_get_phys(virt)) {
                void *phys = pmm_alloc_page();
                memset(phys, 0, PAGE_SIZE);
                vmm_map_page((uintptr_t)phys, virt, page_flags);
            }
        }

        if (filesz > 0 && offset + filesz <= size) {
            memcpy((void*)vaddr, data + offset, filesz);
        }
        if (memsz > filesz) {
            memset((void*)(vaddr + filesz), 0, memsz - filesz);
        }
    }

    *entry_out = (uintptr_t)ehdr->e_entry;
    return 0;
}
