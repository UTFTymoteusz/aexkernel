#include "aex/mem.h"
#include "aex/kernel.h"
#include "aex/spinlock.h"
#include "aex/string.h"
#include "aex/syscall.h"
#include "aex/proc/task.h"

#include "mem/frame.h"

#include "mem/page.h"

// multiple sptrs pls

extern void* PML4;
extern void* PDPT1;
extern void* PDT1,* PDT2;

pagemap_t kernel_pmap;

uint64_t* pgsptr;
uint64_t* pg_page_entry = NULL;

mem_pool_t* page_pointer_structs_pool;

spinlock_t sptr_aim_spinlock = {
    .val = 0,
    .name = "ptr aim spinlock",
};

static inline void aim_pgsptr(phys_addr at) {
    if (pg_page_entry == NULL) {
        pgsptr = (uint64_t*) at;
        return;
    }
    uint64_t addr = (at & MEM_PAGE_MASK) | PAGE_WRITE | PAGE_PRESENT;
    if (*pg_page_entry == addr)
        return;

    *pg_page_entry = addr;

    asm volatile("mov %0, cr3;\
                  mov cr3, %0;" : : "r"(0));
}

void mempg_init() {
    kernel_pmap.vstart = (void*) 0xFFFFFFFF80100000;
    kernel_pmap.vend   = (void*) 0xFFFFFFFFFFFFFFFF;

    kernel_pmap.root_dir   = (phys_addr) &PML4;

    //kp_init_desc(&kernel_pgtrk, (phys_addr) &PML4);
    kernel_pmap.dir_frames_used = 8;
}

void mempg_init2() {
    void* virt_addr = kpmap(1, (phys_addr) 0x100000, NULL, PAGE_WRITE);

    uint64_t virt = (uint64_t) virt_addr;

    uint64_t pml4index = (virt >> 39) & 0x1FF;
    uint64_t pdpindex  = (virt >> 30) & 0x1FF;
    uint64_t pdindex   = (virt >> 21) & 0x1FF;
    uint64_t ptindex   = (virt >> 12) & 0x1FF;

    volatile uint64_t* pml4 = (uint64_t*) kernel_pmap.root_dir;
    volatile uint64_t* pdp  = (uint64_t*) (pml4[pml4index] & ~0xFFF);
    volatile uint64_t* pd   = (uint64_t*) (pdp[pdpindex] & ~0xFFF);
    volatile uint64_t* pt   = (uint64_t*) (pd[pdindex] & ~0xFFF);

    volatile uint64_t* pt_v  = kpmap(1, (phys_addr) pt, NULL, PAGE_WRITE);

    pg_page_entry = (uint64_t*) (pt_v + ptindex);

    pgsptr = (void*) (virt & MEM_PAGE_MASK);
    
    for (int i = 0; i < 256; i++)
        ((uint64_t*) kernel_pmap.root_dir)[i] = 0x0000; // We don't need these anymore

    //syscalls[SYSCALL_PGALLOC] = syscall_pgalloc;
    //syscalls[SYSCALL_PGFREE]  = syscall_pgfree;
}

phys_addr _kppaddrof(void* virt, pagemap_t* proot);

uint64_t* _mempg_find_table(uint64_t virt_addr, pagemap_t* proot, uint64_t* skip) {
    if (proot == NULL)
        proot = &kernel_pmap;
        
    uint64_t pml4index = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpindex  = (virt_addr >> 30) & 0x1FF;
    uint64_t pdindex   = (virt_addr >> 21) & 0x1FF;

    aim_pgsptr(proot->root_dir);
    volatile uint64_t* pml4 = pgsptr;
    if (pml4[pml4index] == 0x0000) {
        *skip += 0x8000000000;
        return NULL;
    }
    aim_pgsptr(pml4[pml4index] & ~0xFFF);
    volatile uint64_t* pdp = pgsptr;
    if (pdp[pdpindex] == 0x0000) {
        *skip += 0x40000000;
        return NULL;
    }
    aim_pgsptr(pdp[pdpindex] & ~0xFFF);
    volatile uint64_t* pd = (uint64_t*) pgsptr;
    if (pd[pdindex] == 0x0000) {
        *skip += 0x200000;
        return NULL;
    }
    aim_pgsptr(pd[pdindex] & ~0xFFF);
    volatile uint64_t* pt = (uint64_t*) pgsptr;

    return (uint64_t*) (((uint64_t) pt) & ~0xFFF);
}

uint64_t* mempg_find_table(uint64_t virt_addr, pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    uint64_t skip = 0;
    return _mempg_find_table(virt_addr, proot, &skip);
}

uint64_t* mempg_find_table_ensure(uint64_t virt_addr, pagemap_t* proot) {
    uint32_t frame_id;
    uint64_t index;

    uint8_t  index_shift = 39;
    bool     reset = false;

    aim_pgsptr(proot->root_dir);

    // Here we iterate over 3 paging levels, the PML4, the PDP and the PD until we reach the PT
    for (int i = 0; i < 3; i++) {
        index = (virt_addr >> index_shift) & 0x01FF;
        index_shift -= 9;

        if (!(pgsptr[index] & PAGE_PRESENT)) {
            frame_id = kfcalloc(1);

            proot->dir_frames_used++;

            pgsptr[index] = ((uint64_t) kfpaddrof(frame_id)) | PAGE_USER | PAGE_WRITE | PAGE_PRESENT;
            reset = true;
        }
        else
            pgsptr[index] |= PAGE_USER | PAGE_WRITE | PAGE_PRESENT;

        aim_pgsptr(pgsptr[index] & MEM_PAGE_MASK);
        if (reset) {
            memset((void*) pgsptr, 0, 0x1000);
            reset = false;
        }
    }
    return (uint64_t*) (((uint64_t) pgsptr) & MEM_PAGE_MASK);
}

#define KPFOREACH_FIRST 2048

struct kpforeach_data {
    int local_index;
    uint64_t* page_table;
};
typedef struct kpforeach_data kpforeach_data_t;

void* _kpforeach_init(void** virt, phys_addr* phys, uint32_t* flags, pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    kpforeach_data_t* fe_data = kmalloc(sizeof(kpforeach_data_t));
    fe_data->local_index = KPFOREACH_FIRST;

    *virt  = proot->vstart;
    *phys  = 0;
    *flags = 0;

    spinlock_acquire(&(proot->spinlock));
    spinlock_acquire(&sptr_aim_spinlock);

    return fe_data;
}

bool _kpforeach_advance(void** virt, phys_addr* phys, uint32_t* flags, pagemap_t* proot, void* data) {
    if (proot == NULL)
        proot = &kernel_pmap;

    kpforeach_data_t* fe_data = data;

    while (fe_data->local_index < 512) {
        *virt += 0x1000;
        fe_data->local_index++;

        if (fe_data->local_index == 512) // This makes sure we transition over to the next PD succesfully
            break;

        *phys = fe_data->page_table[fe_data->local_index];
        if (*phys == 0x0000)
            continue;

        *flags = *phys & ~MEM_PAGE_MASK;
        *phys &= MEM_PAGE_MASK;

        return true;
    }
    uint64_t* pt;

again:
    while (*virt < proot->vend) {
        pt = _mempg_find_table((uint64_t) *virt, proot, (uint64_t*) virt);
        if (pt == NULL)
            continue;

        if (fe_data->local_index == KPFOREACH_FIRST)
            fe_data->local_index = (((size_t) *virt) & 0x1FFFFF) / 0x1000;
        else
            fe_data->local_index = 0;

        fe_data->page_table  = pt;

        *phys = pt[fe_data->local_index];

        while (*phys == 0x0000) {
            fe_data->local_index++;
            if (fe_data->local_index == 512)
                goto again;

            *phys = pt[fe_data->local_index];
        }
        *flags = *phys & ~MEM_PAGE_MASK;
        *phys &= MEM_PAGE_MASK;

        return true;
    }
    kfree(fe_data);

    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));

    return false;
}

void* mempg_find_contiguous(size_t amount, pagemap_t* proot) {
    if (amount == 0)
        return NULL;

    uint64_t index = (((size_t) proot->vstart) >> 12) & 0x1FF;
    uint64_t virt  = (uint64_t) proot->vstart;
    void*    start = proot->vstart;
    size_t   combo = 0;

    uint64_t* tb;

    tb = mempg_find_table(virt, proot);
    if (tb == NULL)
        tb = mempg_find_table_ensure((uint64_t) proot->vstart, proot);

    int counted = index - 1;

    while (true) {
        counted++;

        if (index >= 512) {
            index = 0;

            tb = mempg_find_table(virt, proot);
            if (tb == NULL) {
                index = 512;
                combo += 512;
                virt  += CPU_PAGE_SIZE * 512;

                if (combo >= amount)
                    return start;
        
                continue;
            }
        }
        if (tb[index++] != 0x0000) {
            virt += CPU_PAGE_SIZE;
            start = (void*) virt;
            combo = 0;

            continue;
        }
        combo++;
        if (combo >= amount)
            return start;

        virt += CPU_PAGE_SIZE;
    }
}

void mempg_assign(void* virt, phys_addr phys, pagemap_t* proot, uint16_t flags) {
    if (proot == NULL)
        proot = &kernel_pmap;

    uint64_t virt_addr = (uint64_t) virt;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t phys_addr = (uint64_t) phys;
    phys_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, proot);
    uint64_t  index = (virt_addr >> 12) & 0x1FF;

    table[index] = phys_addr | flags | PAGE_PRESENT;

    asm volatile("mov %0, cr3; \
                  mov cr3, %0;" : : "r"(0));
}

uint16_t mempg_remove(void* virt, pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    uint64_t virt_addr = (uint64_t) virt;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* volatile table = mempg_find_table_ensure(virt_addr, proot);
    uint64_t index = (virt_addr >> 12) & 0x1FF;

    uint16_t flags = table[index] & ~MEM_PAGE_MASK;
    table[index] = 0;

    asm volatile("mov %0, cr3; \
                  mov cr3, %0;" : : "r"(0));

    return flags;
}

void* kpalloc(size_t amount, pagemap_t* proot, uint16_t flags) {
    if (proot == NULL)
        proot = &kernel_pmap;

    if (amount == 0)
        return NULL;

    flags &= 0x0FFF;
        
    spinlock_acquire(&(proot->spinlock));

    uint32_t  frame;
    phys_addr phys_ptr;

    spinlock_acquire(&sptr_aim_spinlock);
}

void* kpcalloc(size_t amount, pagemap_t* proot, uint16_t flags) {
    if (proot == NULL)
        proot = &kernel_pmap;

    if (amount == 0)
        return NULL;

    flags &= 0x0FFF;

    spinlock_acquire(&(proot->spinlock));

    uint32_t frame = kfcalloc(amount);

    spinlock_acquire(&sptr_aim_spinlock);

    phys_addr phys_ptr = kfpaddrof(frame);
    void* virt_ptr = mempg_find_contiguous(amount, proot);

    void* start = virt_ptr;

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, proot, flags);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    proot->frames_used += amount;

    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));
    return start;
}

void* kpvalloc(size_t amount, void* virt, pagemap_t* proot, uint16_t flags) {
    if (proot == NULL)
        proot = &kernel_pmap;

    if (amount == 0)
        return NULL;

    flags &= 0x0FFF;
        
    spinlock_acquire(&(proot->spinlock));

    uint32_t  frame;
    phys_addr phys_ptr;

    spinlock_acquire(&sptr_aim_spinlock);

    void* start = virt;

    for (size_t i = 0; i < amount; i++) {
        frame    = kfcalloc(1);
        phys_ptr = kfpaddrof(frame);

        //printk("0x%016x >> 0x%016x (%i)\n", virt, phys_ptr, frame);

        mempg_assign(virt, phys_ptr, proot, flags);

        virt += MEM_FRAME_SIZE;
    }
    proot->frames_used += amount;

    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));
    return start;
}


void kpfree(void* virt, size_t amount, pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    if (((size_t) virt & 0xFFF) > 0)
        kpanic("free alignment");

    spinlock_acquire(&(proot->spinlock));
    spinlock_acquire(&sptr_aim_spinlock);

    phys_addr paddr;
    uint16_t  flags;

    for (size_t i = 0; i < amount; i++) {
        paddr = _kppaddrof(virt, proot);
        flags = mempg_remove(virt, proot);

        //printk("0x%016p >> 0x%016x, 0b%012b\n", virt, paddr, flags);
        if (!(flags & PAGE_NOPHYS)) {
            kffree(kfindexof(paddr));
            proot->frames_used--;
        }
        else
            proot->map_frames_used--;

        virt += 0x1000;
    }
    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));
}

void kpunmap(void* virt, size_t amount, pagemap_t* proot) {
    kpfree(virt, amount, proot);
}

void* kpmap(size_t amount, phys_addr phys_ptr, pagemap_t* proot, uint16_t flags) {
    if (proot == NULL)
        proot = &kernel_pmap;

    flags &= 0x0FFF;

    spinlock_acquire(&(proot->spinlock));

    phys_addr offset = 0;

    if ((phys_ptr & 0xFFF) > 0) {
        amount++;
        offset = phys_ptr & 0xFFF;
    }
    spinlock_acquire(&sptr_aim_spinlock);

    void* virt_ptr = mempg_find_contiguous(amount, proot);
    void* start    = virt_ptr;

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr - offset, proot, flags | PAGE_NOPHYS);

        //printk("kpmap 0x%016p >> 0x%016x\n", virt_ptr, phys_ptr - offset);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    proot->map_frames_used += amount;

    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));
    return start + offset;
}

void* mempg_mapvto(size_t amount, void* virt_ptr, phys_addr phys_ptr, pagemap_t* proot, uint16_t flags) {
    if (proot == NULL)
        proot = &kernel_pmap;

    flags &= 0x0FFF;

    spinlock_acquire(&(proot->spinlock));

    void* start = virt_ptr;

    //printk("mempg_mapvto() mapped piece %i (virt 0x%016lX), len %i to 0x%016lX\n", piece, (size_t) virt_ptr, amount, (size_t) phys_ptr);
    //for (volatile uint64_t i = 0; i < 45000000; i++);

    spinlock_acquire(&sptr_aim_spinlock);

    for (size_t i = 0; i < amount; i++) {
        mempg_assign(virt_ptr, phys_ptr, proot, flags | PAGE_NOPHYS);

        phys_ptr += MEM_FRAME_SIZE;
        virt_ptr += MEM_FRAME_SIZE;
    }
    proot->map_frames_used += amount;

    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));
    return start;
}

uint64_t kpframeof(void* virt, pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    return kfindexof(kppaddrof(virt, proot));
}

phys_addr _kppaddrof(void* virt, pagemap_t* proot) {
    uint64_t virt_addr = (uint64_t) virt;
    virt_addr &= MEM_PAGE_MASK;

    uint64_t* table = mempg_find_table_ensure(virt_addr, proot);
    uint64_t index  = (virt_addr >> 12) & 0x1FF;

    return (phys_addr) (table[index] & MEM_PAGE_MASK) + (((size_t) virt) & ~MEM_PAGE_MASK);
}

phys_addr kppaddrof(void* virt, pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    spinlock_acquire(&(proot->spinlock));
    spinlock_acquire(&sptr_aim_spinlock);

    phys_addr ret = _kppaddrof(virt, proot);

    spinlock_release(&sptr_aim_spinlock);
    spinlock_release(&(proot->spinlock));
    return ret;
}

phys_addr _mempg_create_dir() {
    spinlock_acquire(&sptr_aim_spinlock);

    phys_addr addr = kfpaddrof(kfcalloc(1));
    aim_pgsptr(addr);

    memset((void*) pgsptr, 0, 0x1000);

    volatile uint64_t* pml4t = pgsptr;
    pml4t[511] = (uint64_t) (&PDPT1) | PAGE_WRITE | PAGE_PRESENT;

    spinlock_release(&sptr_aim_spinlock);
    return addr;
}

void _mempg_dispose_dir(phys_addr addr) {
    spinlock_acquire(&sptr_aim_spinlock);

    uint64_t pdp;
    uint64_t pd;

    for (int pml4i = 0; pml4i < 510; pml4i++) {
        aim_pgsptr(addr);
        if (!(pgsptr[pml4i] & PAGE_PRESENT)) 
            continue;

        pdp = pgsptr[pml4i] & MEM_PAGE_MASK;
        for (int pdpi = 0; pdpi < 512; pdpi++) {
            aim_pgsptr(pdp);
            if (!(pgsptr[pdpi] & PAGE_PRESENT)) 
                continue;

            pd = pgsptr[pdpi] & MEM_PAGE_MASK;
            aim_pgsptr(pd);
            for (int pdi = 0; pdi < 512; pdi++) {
                if (!(pgsptr[pdi] & PAGE_PRESENT)) 
                    continue;

                kffree(kfindexof(pgsptr[pdi] & MEM_PAGE_MASK));
            }
            kffree(kfindexof(pd));
        }
        kffree(kfindexof(pdp));
    }
    kffree(kfindexof(addr));
    spinlock_release(&sptr_aim_spinlock);
}

pagemap_t* kp_create_map() {
    pagemap_t* map = kzmalloc(sizeof(pagemap_t));
    map->root_dir = _mempg_create_dir();

    return map;
}

void kp_dispose_map(pagemap_t* map) {
    _mempg_dispose_dir(map->root_dir);
    kfree(map);
}

uint64_t* _mempg_find_directory(uint64_t virt_addr, phys_addr phys_root, uint64_t* skip) {
    uint64_t pml4index = (virt_addr >> 39) & 0x1FF;
    uint64_t pdpindex  = (virt_addr >> 30) & 0x1FF;
    uint64_t pdindex   = (virt_addr >> 21) & 0x1FF;

    aim_pgsptr(phys_root);
    volatile uint64_t* pml4 = pgsptr;
    if (pml4[pml4index] == 0x0000) {
        *skip += 0x8000000000;
        return NULL;
    }
    aim_pgsptr(pml4[pml4index] & ~0xFFF);
    volatile uint64_t* pdp = pgsptr;
    if (pdp[pdpindex] == 0x0000) {
        *skip += 0x40000000;
        return NULL;
    }
    aim_pgsptr(pdp[pdpindex] & ~0xFFF);
    volatile uint64_t* pd = (uint64_t*) pgsptr;
    if (pd[pdindex] == 0x0000) {
        *skip += 0x200000;
        return NULL;
    }
    return (uint64_t*) (((uint64_t) pd) & ~0xFFF);
}

void kp_change_dir(pagemap_t* proot) {
    if (proot == NULL)
        proot = &kernel_pmap;

    CURRENT_CONTEXT->cr3 = (uint64_t) proot->root_dir;
    asm volatile("mov cr3, %0;" : : "r"(proot->root_dir));
}