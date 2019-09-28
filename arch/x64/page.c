#pragma once

#include "dev/cpu.h"
#include "mem/frame.h"

#include "kernel/debug.h"

#define MEM_PAGE_MASK ~(CPU_PAGE_SIZE - 1)
#define MEM_PAGE_ENTRY_SIZE 16

size_t mempg_kernel_counter = 0xFFFFFFFF80100000;

extern void* PML4;
    
// To do: Chicken-egg situation avoidance with paging
uint64_t* mempg_find_table_ensure(uint64_t virt_addr, void* root) {
    uint64_t pml4index = virt_addr >> 48 & 0x1FF;
    uint64_t pdpindex  = virt_addr >> 39 & 0x1FF;
    uint64_t pdindex   = virt_addr >> 30 & 0x1FF;
    //uint64_t ptindex   = virt_addr >> 21 & 0x1FF;

    //static char boibuffer[24];

    //printf("pml4: %s\n", itoa(pml4index, boibuffer, 10));
    //printf("pdp : %s\n", itoa(pdpindex, boibuffer, 10));
    //printf("pd  : %s\n", itoa(pdindex, boibuffer, 10));
    //printf("pt  : %s\n", itoa(ptindex, boibuffer, 10));

    volatile uint64_t* pml4 = (uint64_t*)root;
    if (!(pml4[pml4index] & 0b0001)) {
        //printf("new_pdp ");
        pml4[pml4index] = (uint64_t)mem_frame_alloc(mem_frame_get_free()) | 0b11111;
    }
    else
        pml4[pml4index] |= 0b11111;

    uint64_t* pdp = (uint64_t*)(pml4[pml4index] & ~0xFFF);
    if (!(pdp[pdpindex] & 0b0001)) {
        //printf("new_pd ");
        pdp[pdpindex] = ((uint64_t)mem_frame_alloc(mem_frame_get_free())) | 0b11111;
    }
    else
        pdp[pdpindex] |= 0b11111;

    uint64_t* pd = (uint64_t*)(pdp[pdpindex] & ~0xFFF);
    if (!(pd[pdindex] & 0b0001)) {
        //printf("new_pt ");
        pd[pdindex] = ((uint64_t)mem_frame_alloc(mem_frame_get_free())) | 0b11111;
    }
    else
        pd[pdindex] |= 0b11111;

    uint64_t* pt = (uint64_t*)(pd[pdindex] & ~0xFFF);
    //if (!(pt[ptindex] & 0b0001)) {
    //    //printf("new_pt ");
    //    pt[ptindex] = ((uint64_t)mem_frame_alloc(mem_frame_get_free())) | 0x07;
    //}
    
    //printf("pml4: 0x%s\n", itoa((uint64_t)pml4, boibuffer, 16));
    //printf("pdp : 0x%s\n", itoa((uint64_t)pdp, boibuffer, 16));
    //printf("pd  : 0x%s\n", itoa((uint64_t)pd, boibuffer, 16));
    //printf("pt  : 0x%s\n", itoa((uint64_t)pt, boibuffer, 16));

    return (uint64_t*)(((uint64_t)pt) & ~0xFFF);
}

void mempg_assign(void* virt, void* phys, void* root, unsigned char flags) {

    if (root == NULL) 
        root = &PML4;

    uint64_t virt_addr = ((uint64_t)virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t phys_addr = (uint64_t)phys;
    phys_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, root);
    uint64_t index = (virt_addr >> 21) & 0x1FF;

    //static char boibuffer[24];
    //printf("phys : 0x%s\n", itoa(phys_addr, boibuffer, 16));
    //printf("virt : 0x%s\n", itoa(virt_addr, boibuffer, 16));
    //printf("pt   : 0x%s\n", itoa((uint64_t)table, boibuffer, 16));
    //printf("boi  : 0x%s\n", itoa(MEM_PAGE_MASK, boibuffer, 16));

    //asm volatile("xchg bx, bx;" : : "a"(MEM_PAGE_MASK));

    table[index] = phys_addr | flags | 1;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}
void mempg_remove(void* virt, void* root) {

    if (root == NULL) 
        root = &PML4;

    uint64_t virt_addr = ((uint64_t)virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, root);
    uint64_t index = (virt_addr >> 21) & 0x1FF;

    table[index] = 0;

    asm volatile("mov rax, cr3;\
                  mov cr3, rax;");
}

void* mempg_next(size_t amount, size_t* counter, void* root, unsigned char flags) {

    if (root == NULL) 
        root = &PML4;
        
    if (counter == NULL)
        counter = &mempg_kernel_counter;

    void* phys_ptr;
    void* start = (void*)*counter;

    for (size_t i = 0; i < amount; i++) {
        phys_ptr = mem_frame_alloc(mem_frame_get_free());
        mempg_assign((void*)*counter, phys_ptr, NULL, flags);

        *counter += MEM_FRAME_SIZE;
    }

    return start;
}
void* mempg_nextc(size_t amount, size_t* counter, void* root, unsigned char flags) {

    if (root == NULL) 
        root = &PML4;
        
    if (counter == NULL)
        counter = &mempg_kernel_counter;

    void* phys_ptr = mem_frame_get_ptr(mem_frame_alloc_contiguous(amount));
    void* start = (void*)*counter;

    for (size_t i = 0; i < amount; i++) {
        mempg_assign((void*)*counter, phys_ptr, NULL, flags);

        phys_ptr += MEM_FRAME_SIZE;
        *counter += MEM_FRAME_SIZE;
    }

    return start;
}
void* mempg_mapto(size_t amount, size_t* counter, void* phys_ptr, void* root, unsigned char flags) {

    if (root == NULL) 
        root = &PML4;
        
    if (counter == NULL)
        counter = &mempg_kernel_counter;

    void* start = (void*)*counter;

    for (size_t i = 0; i < amount; i++) {
        mempg_assign((void*)*counter, phys_ptr, NULL, flags);

        phys_ptr += MEM_FRAME_SIZE;
        *counter += MEM_FRAME_SIZE;
    }

    return start;
}

void* mempg_paddr(void* virt, void* root) {
    if (root == NULL) 
        root = &PML4;

    uint64_t virt_addr = ((uint64_t)virt) << 9;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, root);
    uint64_t index = (virt_addr >> 21) & 0x1FF;

    return (void*)(table[index] & MEM_PAGE_MASK);
}