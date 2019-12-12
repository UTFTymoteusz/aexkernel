#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/time.h"

#include "aex/fs/fs.h"
#include "aex/fs/file.h"

#include "aex/proc/exec.h"

#include "mem/page.h"
#include "mem/pagetrk.h"

#include <stdio.h>
#include <string.h>

#include "elftypes.h"
#include "elfload.h"

const char* elf_magic = "\x7F" "ELF";

int elf_load(char* path, char* args[], struct exec_data* exec, page_tracker_t* tracker) {
    int ret;
    file_t* file = kmalloc(sizeof(file_t));

    memset(exec, 0, sizeof(struct exec_data));

    ret = fs_open(path, file);
    if (ret < 0) {
        kfree(file);
        return ret;
    }
    struct elf_header* header = kmalloc(sizeof(struct elf_header));
    fs_read(file, (uint8_t*) header, sizeof(struct elf_header));

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

    exec->entry = (void*) header->entry;
    exec->pentry = NULL;

    //printf("Entry: 0x%x\n", header->entry);

    /*printf("Pos  : 0x%x\n", phdr_pos);
    printf("Amnt : %i\n", header->prog_hdr_tbl_entry_amount);
    printf("Names: %i\n", header->index_with_section_names);

    sleep(2000);*/

    elf_program_header_t* pheaders = kmalloc(sizeof(elf_program_header_t) * phdr_cnt);

    fs_seek(file, phdr_pos);
    fs_read(file, (uint8_t*) pheaders, sizeof(elf_program_header_t) * phdr_cnt);

    uint64_t size = 0;

    for (uint32_t i = 0; i < phdr_cnt; i++) {
        if (pheaders[i].type != 1)
            continue;

        if ((pheaders[i].addr + pheaders[i].msize) > size)
            size = pheaders[i].addr + pheaders[i].msize;
    }
    void* pg_root = mempg_create_user_root(&(tracker->root_virt));

    tracker->vstart = 0x00000000;

    mempg_init_tracker(tracker, pg_root);

    void* boi = kpcalloc(kptopg(size), tracker, 0x07);
    void* ker = kpcalloc(CPU_ENTRY_CALLER_SIZE + args_memlen(args), tracker, 0x07);

    exec->starting_frame = kpframeof(boi, tracker);
    exec->page_amount = kptopg(size);
    exec->phys_addr   = kppaddrof(0x00, tracker);
    
    void* ker_mem_phys = kppaddrof(ker, tracker);

    void* exec_mem = kpmap(exec->page_amount, exec->phys_addr, NULL, 0x03);
    void* ker_mem  = kpmap(CPU_ENTRY_CALLER_SIZE + args_memlen(args), ker_mem_phys, NULL, 0x03);

    exec->addr = exec_mem;

    uint64_t offset, addr, fsize, msize;

    for (uint32_t i = 0; i < phdr_cnt; i++) {
        if (pheaders[i].type != 1)
            continue;

        //printf("Type : %i\n", pheaders[i].type);
        //printf("Flags: %i\n\n", pheaders[i].flags);

        offset = pheaders[i].offset;
        addr   = pheaders[i].addr;
        fsize  = pheaders[i].fsize;
        msize  = pheaders[i].msize;

        /*printf("off : 0x%x\n", offset);
        printf("addr: 0x%x\n", addr);
        printf("fsize: %i\n", fsize);
        printf("msize: %i\n\n", msize);*/

        memset(exec_mem + addr, 0, msize);

        fs_seek(file, offset);
        fs_read(file, exec_mem + addr, fsize);
    }
    //printf("Loaded ELF\n");

    kfree(file);
    kfree(header);

    exec->ker_proc_addr = (size_t) ker;

    setup_entry_caller(ker_mem, exec->ker_proc_addr, args);
    exec->pentry = ker;

    kpunmap(ker_mem, CPU_ENTRY_CALLER_SIZE + args_memlen(args), NULL);
    kpunmap(exec_mem, exec->page_amount, NULL);
    return 0;
}