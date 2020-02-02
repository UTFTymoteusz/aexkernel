#pragma once

#include <stdint.h>

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
} PACKED;

struct elf_program_header {
    uint32_t type;
    uint32_t flags;

    uint64_t offset;
    uint64_t addr;

    uint64_t no_clue;

    uint64_t fsize;
    uint64_t msize;

    uint64_t alignment;
} PACKED;
typedef struct elf_program_header elf_program_header_t;

struct elf_section_header {
    uint32_t name_index;
    uint32_t type;
    uint64_t flags;

    uint64_t addr;
    uint64_t offset;

    uint64_t size;
    uint32_t link;

    uint32_t info;
    uint64_t alignment;
    uint64_t member_size;
} PACKED;
typedef struct elf_section_header elf_section_header_t;

struct elf_symbol {
    uint32_t name_index;
    uint8_t info;
    uint8_t other;

    uint16_t symbol_index;
    
    uint64_t addr;
    uint64_t size;
} PACKED;
typedef struct elf_symbol elf_symbol_t;