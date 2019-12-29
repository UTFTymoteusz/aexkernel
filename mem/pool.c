#include "aex/aex.h"
#include "aex/time.h"
#include "aex/mutex.h"

#include <stdio.h>
#include <string.h>

#include "aex/mem.h"
#include "frame.h"
#include "page.h"

#include "pool.h"

#define POOL_PIECE_SIZE 16
#define DEFAULT_POOL_SIZE 0x1000 * 256
#define ALLOC_DATATYPE uint32_t

struct mem_pool {
    mutex_t mutex;

    uint32_t pieces;
    uint32_t free_pieces;

    struct mem_pool* next;

    void* start;

    ALLOC_DATATYPE bitmap[];
};
typedef struct mem_pool mem_pool_t;

// idea: make mem blocks shareable (to waste less space)
struct mem_block {
    uint8_t wasted_size;
    uint8_t uses;

    uint16_t cookie;

    uint32_t start_piece;
    uint32_t pieces;
} PACKED;
typedef struct mem_block mem_block_t;

mem_pool_t* mem_pool0;

void mempo_cleanup(mem_pool_t* pool);

inline size_t floor_to_alignment(size_t in) {
    return (in / POOL_PIECE_SIZE) * POOL_PIECE_SIZE;
}

inline size_t ceil_to_alignment(size_t in) {
    return ((in + (POOL_PIECE_SIZE - 1)) / POOL_PIECE_SIZE) * POOL_PIECE_SIZE;
}

mem_pool_t* mempo_create(uint32_t size) {
    size += (size % (POOL_PIECE_SIZE * 2));
    size_t pieces = size / POOL_PIECE_SIZE;

    size_t required_size = size + sizeof(mem_pool_t);
    void* ptr = kpalloc(kptopg(required_size), NULL, 0x03);

    size_t actual_size    = ((kpfrompg(kptopg(required_size)) - sizeof(mem_pool_t)) / POOL_PIECE_SIZE) * POOL_PIECE_SIZE;
    size_t remaining_size = floor_to_alignment(actual_size - (pieces / 8));

    mem_pool_t* pool = ptr;
    pool->pieces = remaining_size / POOL_PIECE_SIZE;
    pool->free_pieces = pool->pieces;
    pool->start = (void*) ceil_to_alignment(((size_t) pool + sizeof(mem_pool_t) + (pieces / 8)));

    pool->next = NULL;

    pool->mutex = 0;
    memset(&(pool->bitmap), 0, pool->pieces / 8);
    memset(pool->start, 0, pool->pieces * POOL_PIECE_SIZE);

    printf("po: 0x%x; bi: 0x%x; st: 0x%x;\n", (size_t) ptr & 0xFFFFFFFFFFFF, (size_t) &(pool->bitmap) & 0xFFFFFFFFFFFF, (size_t) pool->start & 0xFFFFFFFFFFFF);
    if (checkinterrupts())
        sleep(10000);

    return pool;
}

void mempo_init() {
	mem_pool0 = mempo_create(DEFAULT_POOL_SIZE);
	printf("Created initial %i KB kernel memory pool\n\n", DEFAULT_POOL_SIZE / 1000);
}

static inline int64_t find_space(mem_pool_t* pool, size_t piece_amount) {
    if (piece_amount == 0)
        return -1;

    size_t  combo = 0;
    int64_t start = -1;
    size_t remaining = pool->pieces;

    int32_t current_index = 0;
    uint16_t bit = 0;

    size_t bitmap_piece = pool->bitmap[current_index];
    
    while (remaining-- > 0) {
        if (bit >= (sizeof(ALLOC_DATATYPE) * 8)) {
            bitmap_piece = pool->bitmap[++current_index];
            bit = 0;
        }

        if (((bitmap_piece >> bit) & 0b01) == 1) {
            start = -1;
            combo = 0;

            bit++;
            continue;
        }

        if (start == -1)
            start = current_index * (sizeof(ALLOC_DATATYPE) * 8) + bit;

        bit++;
        combo++;
        if (combo == piece_amount)
            return start;
    }
    //printf("failed\n");
    return -1;
}

static inline void set_pieces(mem_pool_t* pool, size_t starting_piece, size_t amount, bool occupied) {
    if (amount == 0)
        return;

    uint32_t current_index = starting_piece / (sizeof(ALLOC_DATATYPE) * 8);
    uint16_t bit = starting_piece % (sizeof(ALLOC_DATATYPE) * 8);

    ALLOC_DATATYPE bitmap_piece = pool->bitmap[current_index];

    if (occupied) {
        while (amount-- > 0) {
            if (bit >= (sizeof(ALLOC_DATATYPE) * 8)) {
                pool->bitmap[current_index] = bitmap_piece;
                bitmap_piece = pool->bitmap[++current_index];
                bit = 0;
            }
            if (bitmap_piece & (1 << bit)) {
                printf("mempool: R: %i, CI: %i\n", amount, current_index);
                kpanic("mempool: Attempt to occupy an already-occupied piece");
            }
            bitmap_piece |= (1 << bit++);
        }
    }
    else {
        while (amount-- > 0) {
            if (bit >= (sizeof(ALLOC_DATATYPE) * 8)) {
                pool->bitmap[current_index] = bitmap_piece;
                bitmap_piece = pool->bitmap[++current_index];
                bit = 0;
            }
            if (!(bitmap_piece & (1 << bit))){
                printf("mempool: R: %i\n", amount);
                kpanic("mempool: Attempt to free an already-freed piece");
            }
            bitmap_piece &= ~(1 << bit++);
        }
    }
    pool->bitmap[current_index] = bitmap_piece;
}

static inline size_t addr_to_piece(mem_pool_t* pool, void* addr) {
    return (size_t) (addr - pool->start) / POOL_PIECE_SIZE;
}

static inline mem_pool_t* find_parent(void* addr) {
    mem_pool_t* current = mem_pool0;
    size_t cmp;
    while (current != NULL) {
        cmp = (size_t) (addr - current->start);
        if (cmp <= (current->pieces * POOL_PIECE_SIZE) && addr >= current->start)
            return current;

        current = current->next;
    }
    return NULL;
}

static inline mem_block_t* get_block_from_ext_addr(void* addr) {
    return (mem_block_t*) (floor_to_alignment((size_t) addr) - POOL_PIECE_SIZE);
}

void verify_pools(char* id) {
    mem_pool_t* pool = mem_pool0;

    size_t  combo = 0;
    int64_t start = -1;
    size_t remaining = pool->pieces;

    int32_t current_index = 0;
    uint16_t bit = 0;

    uint8_t* addr;

    ALLOC_DATATYPE bitmap_piece = pool->bitmap[current_index];

    while (remaining-- > 0) {
        if (bit >= (sizeof(ALLOC_DATATYPE) * 8)) {
            bitmap_piece = pool->bitmap[++current_index];
            bit = 0;
        }

        if (((bitmap_piece >> bit) & 0b01) == 1) {
            bit++;
            continue;
        }

        addr = (uint8_t*) (pool->start + (current_index * (sizeof(ALLOC_DATATYPE) * 8) + bit) * POOL_PIECE_SIZE);
        for (size_t i = 0; i < POOL_PIECE_SIZE; i++) {
            if (addr[i] != '\0') {
                printf("mem: %s\n", id);
                kpanic("mem corrupted");
            }
        }
        bit++;
        combo++;
    }
}

void* kmalloc(uint32_t size) {
    if (size == 0)
        return NULL;

    mem_pool_t* pool = mem_pool0;

    uint32_t oldsize = size;
    
    size = ceil_to_alignment(size);
    size_t pieces = (size / POOL_PIECE_SIZE) + (ceil_to_alignment(sizeof(mem_block_t)) / POOL_PIECE_SIZE);
    
    int64_t starting_piece = 0;

    mutex_acquire_yield(&(pool->mutex));
    while (true) {
        if (pool->free_pieces < pieces || starting_piece == -1) {
            if (pool->next == NULL)
                pool->next = mempo_create((size > DEFAULT_POOL_SIZE) ? size : DEFAULT_POOL_SIZE);

            mutex_release(&(pool->mutex));
            pool = pool->next;
            mutex_acquire_yield(&(pool->mutex));
        }

        starting_piece = find_space(pool, pieces);
        if (starting_piece == -1)
            continue;

        set_pieces(pool, starting_piece, pieces, true);
        pool->free_pieces -= pieces;
        break;
    }
    mutex_release(&(pool->mutex));

    void* addr = (void*) ((size_t) pool->start + ((starting_piece + 1) * POOL_PIECE_SIZE));

    uint8_t* test = (uint8_t*) get_block_from_ext_addr(addr);
    for (size_t i = 0; i < pieces * POOL_PIECE_SIZE; i++) {
        if (*test != 0)
            kpanic("not blank");

        test++;
    }
    mem_block_t* block = get_block_from_ext_addr(addr);
    block->start_piece = starting_piece;
    block->pieces = pieces - 1;
    block->uses   = 1;
    block->wasted_size = 0;
    block->cookie = 0xD6A4;

    return addr;
}

void* krealloc(void* space, uint32_t size) {
    if (space == NULL) {
        if (size == 0)
            return NULL;

        return kmalloc(size);
    }
    if (size == 0) {
        kfree(space);
        return NULL;
    }
    mem_block_t* block = get_block_from_ext_addr(space);
    uint64_t oldsize   = block->pieces * POOL_PIECE_SIZE;

    void* new = kmalloc(size);

    memcpy(new, space, oldsize);
    kfree(space);

    return new;
}

void kfree(void* space) {
    mem_block_t* block = get_block_from_ext_addr(space);
    mem_pool_t* parent = find_parent(block);

    mutex_acquire_yield(&(parent->mutex));

    set_pieces(parent, addr_to_piece(parent, block), block->pieces + 1, false);
    parent->free_pieces += block->pieces + 1;
    memset((void*) block, 0, (block->pieces + 1) * POOL_PIECE_SIZE);
    mutex_release(&(parent->mutex));
}