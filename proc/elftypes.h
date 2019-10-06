#pragma once

struct elf_header {
    char    magic_number[4];
    uint8_t bits;
    uint8_t endianiness;
    uint8_t elf_header_version;
    uint8_t abi;

    uint8_t  padding[8];
    uint16_t type;
    uint16_t isa;
    uint32_t elf_version;

    uint64_t entry;
    uint64_t prog_header_table_pos;
    uint64_t sect_header_table_pos;

    uint32_t flags;
    uint16_t header_size;

    uint16_t prog_hdr_tbl_entry_size;
    uint16_t prog_hdr_tbl_entry_amount;
    uint16_t sect_hdr_tbl_entry_size;
    uint16_t sect_hdr_tbl_entry_amount;

    uint16_t index_with_section_names;
};
struct elf_program_header {
    uint32_t type;
    uint32_t flags;
    
    uint64_t offset;
    uint64_t addr;

    uint64_t no_clue;

    uint64_t fsize;
    uint64_t msize;

    uint64_t alignment;
};
typedef struct elf_program_header elf_program_header_t;