#pragma once

#include "aex/dev/cpu.h"
#include "mem/pagetrk.h"

#include <stdbool.h>
#include <stdint.h>

//** HEAP **//
// Allocates the specified amount of bytes on the heap
void* kmalloc (uint32_t size);
// Resizes allocated space, returns a pointer to the resized space
void* krealloc(void* space, uint32_t size);
// Frees up allocated space
void  kfree(void* space);

//** PAGING **//
// Returns the number of pages required to fit the specified amount of bytes.
static inline size_t kptopg(size_t bytes) {
    return (bytes + (CPU_PAGE_SIZE - 1)) / CPU_PAGE_SIZE;
}

// Returns the number of bytes that the specified amount of pages accupies.
static inline size_t kpfrompg(size_t pages) {
    return pages * CPU_PAGE_SIZE;
}

/*
 * Allocates an amount of pages. 
 * Returns the allocated virtual address.
 */
void* kpalloc(size_t amount, page_tracker_t* tracker, uint8_t flags);
/**
 * Allocates an physically contiguous amount of pages. 
 * Returns the allocated virtual address.
 */
void* kpcalloc(size_t amount, page_tracker_t* tracker, uint8_t flags);
// Frees pages and unallocates the related frames, starting at the specified id. It's important to differentiate between kpfree() and kpunmap().
void  kpfree(void* virt, size_t amount, page_tracker_t* tracker);

// Maps the requested size in bytes to the specified physical address. Returns the allocated virtual address.
void* kpmap(size_t amount, void* phys_ptr, page_tracker_t* tracker, uint8_t flags);
// Unassociates frames, starting at the specified id. It's important to differentiate between kpunmap() and kpfree().
void  kpunmap(void* virt, size_t amount, page_tracker_t* tracker);

// Returns the physical address of a virtual address.
void* kppaddrof(void* virt, page_tracker_t* tracker);
// Returns the page index of a virtual address.
uint64_t kpindexof(void* virt, page_tracker_t* tracker);
// Returns the frame id of a virtual address, or 0xFFFFFFFF if it's an arbitrary mapping (e.g. from kpmap()).
uint64_t kpframeof(void* virt, page_tracker_t* tracker);


//** FRAMES **//
// Allocates the specified frame id.
void*    kfalloc(uint32_t frame_id);
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
void* kfpaddrof(uint32_t frame_id);


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