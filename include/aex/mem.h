#pragma once

#include "aex/dev/cpu.h"
#include "mem/pagetrk.h"

#include "page_arch.h"

#include <stdbool.h>
#include <stdint.h>

#define ALLOC_DATATYPE uint32_t

typedef size_t phys_addr;

//** HEAP **//

// Heap memory pool struct
struct mem_pool {
    mutex_t mutex;

    uint32_t pieces;
    uint32_t free_pieces;
    uint32_t reserved_pieces;

    struct mem_pool* next;

    char* name;
    void* start;

    bool ignore_mutex;
    
    ALLOC_DATATYPE bitmap[];
};
typedef struct mem_pool mem_pool_t;

// Creates a heap memory pool
mem_pool_t* kmpool_create(uint32_t size, char* name);

// Allocates the specified amount of bytes on the heap
void* kmalloc (uint32_t size);
// kmalloc(), but zeroes out the allocated memory also
void* kzmalloc (uint32_t size);
// Resizes allocated space, returns a pointer to the resized space
void* krealloc(void* space, uint32_t size);
// Frees up allocated space
void  kfree   (void* space);

// Exactly like kmalloc(), but allows you to specify a pool of your own
void* _kmalloc (uint32_t size, mem_pool_t* pool);
// Exactly like krealloc(), but allows you to specify a pool of your own
void* _krealloc(void* space, uint32_t size, mem_pool_t* pool);
// Exactly like kfree(), but allows you to specify a pool of your own
void  _kfree   (void* space, mem_pool_t* pool);


//** PAGING **//

// Returns the number of pages required to fit the specified amount of bytes.
static inline size_t kptopg(size_t bytes) {
    return (bytes + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE;
}

// Returns the number of bytes that the specified amount of pages occupies.
static inline size_t kpfrompg(size_t pages) {
    return pages * CPU_PAGE_SIZE;
}

/*
 * Allocates an amount of pages. 
 * Returns the allocated virtual address.
 */
void* kpalloc(size_t amount, page_root_t* proot, uint16_t flags);
/**
 * Allocates an physically contiguous amount of pages. 
 * Returns the allocated virtual address.
 */
void* kpcalloc(size_t amount, page_root_t* proot, uint16_t flags);
/**
 * Allocates an amount of pages starting at the specified address.
 * Returns the passed address.
 */
void* kpvalloc(size_t amount, void* virt, page_root_t* proot, uint16_t flags);
// Frees pages and unallocates the related frames, starting at the specified id. It's important to differentiate between kpfree() and kpunmap().
void  kpfree(void* virt, size_t amount, page_root_t* proot);

// Maps the requested size in pages to the specified physical address. Returns the allocated virtual address.
void* kpmap(size_t amount, phys_addr phys_ptr, page_root_t* proot, uint16_t flags);
// Unassociates frames, starting at the specified id. It's important to differentiate between kpunmap() and kpfree().
void  kpunmap(void* virt, size_t amount, page_root_t* proot);

// Returns the physical address of a virtual address.
phys_addr kppaddrof(void* virt, page_root_t* proot);
// Returns the page index of a virtual address.
//uint64_t kpindexof(void* virt, page_root_t* proot);
// Returns the frame id of a virtual address, or 0xFFFFFFFF if it's an arbitrary mapping (e.g. from kpmap()).
uint64_t kpframeof(void* virt, page_root_t* proot);

void* _kpforeach_init(void** virt, phys_addr* phys, uint32_t* flags, page_root_t* proot);
bool _kpforeach_advance(void** virt, phys_addr* phys, uint32_t* flags, page_root_t* proot, void* data);

#define kpforeach(virt, phys, flags, proot) for (void* _kpforeach_local_data = _kpforeach_init(&virt, &phys, &flags, proot); _kpforeach_advance(&virt, &phys, &flags, proot, _kpforeach_local_data); )

//** FRAMES **//

// Allocates the specified frame id.
phys_addr    kfalloc(uint32_t frame_id);
// Allocates a physically contiguous amount of frames. Returns the id of the first allocated frame.
uint32_t kfcalloc(uint32_t amount);
// Unallocates the specified frame id.
bool     kffree(uint32_t frame_id);

// Finds a free frame id.
uint64_t kfamount();
uint64_t kfused();

// Checks if a frame is taken.
bool  kfisfree(uint32_t frame_id);

// Gets the physical address of a frame.
phys_addr kfpaddrof(uint32_t frame_id);
// Gets the frame index of a physical address.
uint32_t kfindexof(phys_addr addr);


//** GENERAL **//
static inline void mcleanup(void* aa) {
    if (aa == NULL)
        return;
        
    kfree((void*) *((size_t*) aa));
}
#define CLEANUP __attribute__((cleanup(mcleanup)))
#define FORMER_LEANUP 

//** TEMP **//
void verify_pools(char* id);