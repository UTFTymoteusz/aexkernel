#include <stddef.h>
#include <stdint.h>

#include "aex/fs/fd.h"
#include "aex/fs/fs.h"

#include "aex/aex.h"
#include "aex/debug.h"
#include "aex/kernel.h"
#include "aex/kslist.h"
#include "aex/mem.h"
#include "aex/string.h"

#include "elftypes.h"

struct stackframe {
    struct stackframe* rbp;
    uint64_t rip;
} PACKED;
typedef struct stackframe stackframe_t;

struct kernel_symbol {
    struct kernel_symbol* next;
    void* addr;
    size_t size;
    char name[];
};
typedef struct kernel_symbol kernel_symbol_t;

void debug_print_registers() {
    uint64_t reg;
    uint16_t reg_s;
    
    asm volatile("mov %0, cr0" : "=r"(reg));
    printk("CR0: 0x%016lX ", reg);
    printk("CR1: #ud :) ");

    asm volatile("mov %0, cr2" : "=r"(reg));
    printk("CR2: 0x%016lX ", reg);

    asm volatile("mov %0, cr3" : "=r"(reg));
    printk("CR3: 0x%016lX\n", reg);


    asm volatile("pushf; pop %0" : "=r"(reg));
    printk("RFLAGS: 0x%016lX\n", reg);

    
    asm volatile("mov %0, cs" : "=r"(reg_s));
    printk("CS: 0x%04X ", reg_s);

    asm volatile("mov %0, ds" : "=r"(reg_s));
    printk("DS: 0x%04X ", reg_s);
    
    asm volatile("mov %0, es" : "=r"(reg_s));
    printk("ES: 0x%04X ", reg_s);

    asm volatile("mov %0, ss" : "=r"(reg_s));
    printk("SS: 0x%04X ", reg_s);
    
    asm volatile("mov %0, fs" : "=r"(reg_s));
    printk("FS: 0x%04X ", reg_s);

    asm volatile("mov %0, gs" : "=r"(reg_s));
    printk("GS: 0x%04X\n", reg_s);

    asm volatile("mov %0, rsp" : "=r"(reg));
    printk("RSP: 0x%016lX\n", reg);
}

void debug_stacktrace() {
    stackframe_t* stkfr;
    asm volatile("mov %0, rbp" : "=r"(stkfr));

    char* name;
    
    printk("Stack trace:\n");
    while (stkfr != NULL) {
        name = debug_resolve_symbol((void*) stkfr->rip);

        printk(" 0x%016lX <%s>\n", stkfr->rip, name == NULL ? "no clue" : name);
        stkfr = stkfr->rbp;
    }
}

void* kernel_image_section_names;
void* kernel_image_strings;

kernel_symbol_t* symbol_list = NULL;

void debug_load_symbols() {
    int fd = fs_open("/sys/aexkrnl.elf", 0);

    struct elf_header header;
    fd_read(fd, (uint8_t*) &header, sizeof(struct elf_header));

    uint64_t phdr_pos = header.prog_header_table_pos;
    uint64_t phdr_cnt = header.prog_hdr_tbl_entry_amount;

    uint64_t shdr_pos = header.sect_header_table_pos;
    uint64_t shdr_cnt = header.sect_hdr_tbl_entry_amount;

    uint16_t sect_names = header.index_with_section_names;

    CLEANUP elf_program_header_t* pheaders = kmalloc(sizeof(elf_program_header_t) * phdr_cnt);
    fd_seek(fd, phdr_pos, SEEK_SET);
    fd_read(fd, (uint8_t*) pheaders, sizeof(elf_program_header_t) * phdr_cnt);
    
    CLEANUP elf_section_header_t* sheaders = kmalloc(sizeof(elf_section_header_t) * shdr_cnt);
    fd_seek(fd, shdr_pos, SEEK_SET);
    fd_read(fd, (uint8_t*) sheaders, sizeof(elf_section_header_t) * shdr_cnt);

    kernel_image_section_names = kmalloc(sheaders[sect_names].size);
    fd_seek(fd, sheaders[sect_names].offset, SEEK_SET);
    fd_read(fd, kernel_image_section_names, sheaders[sect_names].size);

    char* tstr;

    for (size_t i = 0; i < shdr_cnt; i++) {
        tstr = kernel_image_section_names + sheaders[i].name_index;

        if (strcmp(tstr, ".strtab") != 0)
            continue;

        kernel_image_strings = kmalloc(sheaders[i].size);

        fd_seek(fd, sheaders[i].offset, SEEK_SET);
        fd_read(fd, kernel_image_strings, sheaders[i].size);
    }

    for (size_t i = 0; i < shdr_cnt; i++) {
        tstr = kernel_image_section_names + sheaders[i].name_index;

        if (strcmp(tstr, ".symtab") != 0)
            continue;

        fd_seek(fd, sheaders[i].offset, SEEK_SET);

        elf_symbol_t elf_symbol;
        char* sstr;
        kernel_symbol_t* current_symbol = NULL;

        uint32_t si = 0;
        while (si < sheaders[i].size) {
            fd_read(fd, (uint8_t*) &elf_symbol, sizeof(elf_symbol));
            sstr = kernel_image_strings + elf_symbol.name_index;

            si += sizeof(elf_symbol_t);

            if (symbol_list == NULL) {
                symbol_list = kzmalloc(sizeof(kernel_symbol_t) + strlen(sstr) + 1);
                current_symbol = symbol_list;
            }
            else {
                current_symbol->next = kzmalloc(sizeof(kernel_symbol_t) + strlen(sstr) + 1);
                current_symbol = current_symbol->next;
            }
            strcpy((char*) current_symbol->name, sstr);

            current_symbol->addr = (void*) elf_symbol.addr;
            current_symbol->size = elf_symbol.size;
        }
    }
    fd_close(fd);
}

char* debug_resolve_symbol(void* addr) {
    kernel_symbol_t* current_symbol = symbol_list;

    while (current_symbol != NULL) {
        if (current_symbol->addr <= addr && (current_symbol->addr + current_symbol->size) > addr)
            return current_symbol->name;

        current_symbol = current_symbol->next;
    }
    return NULL;
}