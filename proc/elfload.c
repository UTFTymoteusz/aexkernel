#pragma once

#include "elftypes.h"

#include "fs/fs.h"

#include "aex/rcode.h"

const char* elf_magic = "\x7F" "ELF";

int elf_load(char* path) {
    int ret;
    file_t* file = kmalloc(sizeof(file_t));

    ret = fs_fopen(path, file);
    if (ret < 0) {
        kfree(file);
        return ret;
    }
    struct elf_header* header = kmalloc(sizeof(struct elf_header));
    fs_fread(file, (uint8_t*)header, sizeof(file_t));

    if (memcmp(header->magic_number, elf_magic, 4)) {
        kfree(file);
        kfree(header);
        return EXE_ERR_INVALID_FILE;
    }
    if (header->bits != 2 || header->endianiness != 1 || header->isa != 0x3E
     || header->prog_hdr_tbl_entry_size != 56) 
    {
        kfree(file);
        kfree(header);
        return EXE_ERR_INVALID_FILE;
    }
    uint64_t phdr_pos = header->prog_header_table_pos;
    uint64_t phdr_cnt = header->prog_hdr_tbl_entry_amount;

    printf("Entry: 0x%x\n", header->entry);

    printf("Pos  : 0x%x\n", phdr_pos);
    printf("Amnt : %i\n", header->prog_hdr_tbl_entry_amount);
    printf("Names: %i\n", header->index_with_section_names);

    sleep(5000);

    elf_program_header_t* pheaders = kmalloc(sizeof(elf_program_header_t) * phdr_cnt);

    fs_fseek(file, phdr_pos);
    fs_fread(file, (uint8_t*)pheaders, sizeof(elf_program_header_t) * phdr_cnt);

    for (uint32_t i = 0; i < phdr_cnt; i++) {
        if (pheaders[i].type == 0)
            continue;

        printf("Type : %i\n", pheaders[i].type);
        printf("Flags: %i\n\n", pheaders[i].flags);

        printf("off : %x\n", pheaders[i].offset);
        printf("addr: %x\n", pheaders[i].addr);
        printf("fsize: %i\n", pheaders[i].fsize);
        printf("msize: %i\n", pheaders[i].msize);

        sleep(5000);
    }
    printf("Loaded ELF\n");

    return 0;
}