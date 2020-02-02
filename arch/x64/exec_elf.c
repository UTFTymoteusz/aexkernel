#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/rcode.h"
#include "aex/time.h"
#include "aex/string.h"

#include "aex/fs/fs.h"
#include "aex/fs/file.h"

#include "aex/proc/exec.h"

#include "mem/page.h"

#include "elftypes.h"
#include "exec_elf.h"

const char* elf_magic = "\x7F" "ELF";

bool elf_is_safe(elf_program_header_t* pheaders, uint32_t phdr_cnt, paging_descriptor_t* proot) {
    uint64_t addr, msize;
    uint32_t pg_msize;
    
    for (uint32_t i = 0; i < phdr_cnt; i++) {
        if (pheaders[i].type != 1)
            continue;

        addr  = pheaders[i].addr;
        msize = pheaders[i].msize;

        pg_msize = kptopg(msize);

        if (addr > (size_t) proot->vend || addr + pg_msize * CPU_PAGE_SIZE > (size_t) proot->vend)
            return false;
    }
    return true;
}

int elf_load(char* path, char* args[], struct exec_data* exec, paging_descriptor_t* proot) {
    CLEANUP file_t* file = kmalloc(sizeof(file_t));

    memset(exec, 0, sizeof(struct exec_data));

    int ret = fs_open(path, file);
    RETURN_IF_ERROR(ret);

    struct elf_header header;
    fs_read(file, (uint8_t*) &header, sizeof(struct elf_header));

    if (memcmp(header.magic_number, elf_magic, 4))
        return ERR_INVALID_EXE;

    if (header.bits != 2 || header.endianiness != 1 || header.isa != 0x3E || header.prog_hdr_tbl_entry_size != 56) 
        return ERR_INVALID_EXE;

    uint64_t phdr_pos = header.prog_header_table_pos;
    uint64_t phdr_cnt = header.prog_hdr_tbl_entry_amount;

    exec->entry = (void*) header.entry;
    exec->kernel_entry = 0x0000;

    //printk("Entry: 0x%X\n", header->entry);

    /*printk("Pos  : 0x%X\n", phdr_pos);
    printk("Amnt : %i\n", header->prog_hdr_tbl_entry_amount);
    printk("Names: %i\n", header->index_with_section_names);*/

    CLEANUP elf_program_header_t* pheaders = kmalloc(sizeof(elf_program_header_t) * phdr_cnt);

    fs_seek(file, phdr_pos);
    fs_read(file, (uint8_t*) pheaders, sizeof(elf_program_header_t) * phdr_cnt);

    phys_addr pg_root_dir = kp_create_dir();

    proot->vstart = (void*) 0x00000000;
    proot->vend = (void*) 0x7FFFFFFFFFFF;

    kp_init_desc(proot, pg_root_dir);

    if (!elf_is_safe(pheaders, phdr_cnt, proot)) {
        kp_dispose_dir(proot->root_dir);
        return ERR_INVALID_EXE;
    }
    exec->page_amount = 0;
    exec->phys_addr   = kppaddrof(0x00, proot);

    uint64_t offset, addr, fsize, msize;
    uint32_t pg_fsize, pg_msize;

    uint64_t fsize_remaining;
    
    void* section_ptr;

    for (uint32_t i = 0; i < phdr_cnt; i++) {
        if (pheaders[i].type != 1)
            continue;

        offset = pheaders[i].offset;
        addr   = pheaders[i].addr;
        fsize  = pheaders[i].fsize;
        msize  = pheaders[i].msize;

        fsize_remaining = fsize;

        pg_fsize = kptopg(fsize);
        pg_msize = kptopg(msize);

        //printk("off : 0x%016lX\n  addr: 0x%016lX\n  fsize: %li\n  msize: %li\n\n", offset, addr, fsize, msize);

        //printk("pg_msize: %i\n", pg_msize);
        
        section_ptr = kpvalloc(pg_msize, (void*) addr, proot, PAGE_USER | PAGE_WRITE);

        void* mapped;

        fs_seek(file, offset);
        for (uint32_t i = 0; i < pg_msize; i++) {
            //printk("AA 0x%016p >> 0x%016x\n", section_ptr, kppaddrof(section_ptr, proot));
            mapped = kpmap(1, kppaddrof(section_ptr, proot), NULL, PAGE_WRITE);

            memset(mapped, 0, CPU_PAGE_SIZE);

            if (i < pg_fsize) {
                fs_read(file, mapped, (fsize_remaining < CPU_PAGE_SIZE) ? fsize_remaining : CPU_PAGE_SIZE);
                //printk("cpy: 0x%16p\n", section_ptr);

                fsize_remaining -= CPU_PAGE_SIZE;
            }
            section_ptr += CPU_PAGE_SIZE;

            kpunmap(mapped, 1, NULL);
        }
    }
    void* ker     = kpcalloc(kptopg(CPU_ENTRY_CALLER_SIZE + args_memlen(args)), proot, PAGE_USER | PAGE_WRITE);
    void* ker_mem = kpmap(kptopg(CPU_ENTRY_CALLER_SIZE + args_memlen(args)), kppaddrof(ker, proot), NULL, PAGE_WRITE);

    exec->init_code_addr = (size_t) ker;

    setup_entry_caller(ker_mem, exec->init_code_addr, args);
    exec->kernel_entry = ker;

    kpunmap(ker_mem, kptopg(CPU_ENTRY_CALLER_SIZE + args_memlen(args)), NULL);

    return 0;
}