#include "aex/aex.h"
#include "aex/kernel.h"
#include "aex/mutex.h"
#include "aex/string.h"
#include "aex/sys.h"
#include "aex/time.h"

#include "aex/mem.h"
#include "frame.h"
#include "page.h"

#include "pool.h"

#define POOL_PIECE_SIZE  16
#define DEFAULT_POOL_SIZE 0x1000 * 256

struct mem_pool; // mem_pool is defined in aex/mem.h
typedef struct mem_pool mem_pool_t;

struct mem_block {
    uint8_t wasted_size;
    uint8_t uses;

    uint16_t cookie;

    uint32_t start_piece;
    uint32_t pieces;
};
typedef struct mem_block mem_block_t;

mem_pool_t* mem_pool0;

void mempo_cleanup(mem_pool_t* pool);

inline size_t floor_to_alignment(size_t in) {
    return (in / POOL_PIECE_SIZE) * POOL_PIECE_SIZE;
}

inline size_t ceil_to_alignment(size_t in) {
    return ((in + (POOL_PIECE_SIZE - 1)) / POOL_PIECE_SIZE) * POOL_PIECE_SIZE;
}

mem_pool_t* kmpool_create(uint32_t size, char* name) {
    size += (size % (POOL_PIECE_SIZE * 2));
    
    size_t pieces = size / POOL_PIECE_SIZE;
    size_t required_size = size + sizeof(mem_pool_t);

    void* ptr = kpalloc(kptopg(required_size), NULL, PAGE_WRITE);

    size_t actual_size    = ((kpfrompg(kptopg(required_size)) - sizeof(mem_pool_t)) / POOL_PIECE_SIZE) * POOL_PIECE_SIZE;
    size_t remaining_size = floor_to_alignment(actual_size - (pieces / 8));

    mem_pool_t* pool = ptr;

    pool->pieces = remaining_size / POOL_PIECE_SIZE;
    pool->free_pieces = pool->pieces;
    pool->start = (void*) ceil_to_alignment(((size_t) pool + sizeof(mem_pool_t) + (pieces / 8)));
    pool->name  = name;
    pool->next  = NULL;
    pool->mutex.val  = 0;
    pool->mutex.name = "mem pool";

    memset(&(pool->bitmap), 0, pool->pieces / 8);
    memset(pool->start, 0, pool->pieces * POOL_PIECE_SIZE);

    return pool;
}

void mempo_init() {
	mem_pool0 = kmpool_create(DEFAULT_POOL_SIZE, "Root");
	printk(PRINTK_OK "Created initial %i KB kernel memory pool\n\n", DEFAULT_POOL_SIZE / 1000);
}

static inline int64_t find_space(mem_pool_t* pool, size_t piece_amount) {
    if (piece_amount == 0)
        return -1;

    int64_t start = -1;
    size_t  combo =  0;
    size_t remaining = pool->pieces;

    int32_t current_index = 0;
    uint16_t bit = 0;

    size_t bitmap_piece = pool->bitmap[current_index];

    while (remaining-- > 0) {
        if (bit >= sizeof(ALLOC_DATATYPE) * 8) {
            bitmap_piece = pool->bitmap[++current_index];
            bit = 0;
        }

        if (((bitmap_piece >> bit) & 1) == true) {
            start = -1;
            combo = 0;

            bit++;
            
            continue;
        }

        if (start == -1)
            start = current_index * sizeof(ALLOC_DATATYPE) * 8 + bit;

        bit++;

        if (++combo == piece_amount)
            return start;
    }
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
            if (bit >= sizeof(ALLOC_DATATYPE) * 8) {
                pool->bitmap[current_index] = bitmap_piece;
                bitmap_piece = pool->bitmap[++current_index];
                bit = 0;
            }
            if (bitmap_piece & (1 << bit)) {
                printk("mempool: R: %li, CI: %i\n", amount, current_index);
                kpanic("mempool: Attempt to occupy an already-occupied piece");
            }
            bitmap_piece |= (1 << bit++);
        }
    }
    else {
        while (amount-- > 0) {
            if (bit >= sizeof(ALLOC_DATATYPE) * 8) {
                pool->bitmap[current_index] = bitmap_piece;
                bitmap_piece = pool->bitmap[++current_index];
                bit = 0;
            }
            if (!(bitmap_piece & (1 << bit))){
                printk("mempool: R: %li\n", amount);
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

static inline mem_pool_t* find_parent(void* addr, mem_pool_t* root) {
    mem_pool_t* current_pool = root;
    size_t local_addr;
    
    while (current_pool != NULL) {
        local_addr = (size_t) (addr - current_pool->start);

        if (local_addr <= current_pool->pieces * POOL_PIECE_SIZE && addr >= current_pool->start)
            return current_pool;

        current_pool = current_pool->next;
    }
    return NULL;
}

static inline mem_block_t* get_block_from_ext_addr(void* addr) {
    return (mem_block_t*) (floor_to_alignment((size_t) addr) - POOL_PIECE_SIZE);
}

void verify_pools(char* id) {
    mem_pool_t* pool = mem_pool0;

    size_t combo = 0;
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
                printk("mem: %s\n", id);
                kpanic("mem corrupted");
            }
        }

        bit++;
        combo++;
    }
}

void* _kmalloc(uint32_t size, mem_pool_t* pool) {
    if (size == 0)
        return NULL;

    size = ceil_to_alignment(size);
    size_t pieces = (size / POOL_PIECE_SIZE) + (ceil_to_alignment(sizeof(mem_block_t)) / POOL_PIECE_SIZE);
    
    int64_t starting_piece = 0;

    mutex_acquire_yield(&(pool->mutex));
        
    while (true) {
        if (pool->free_pieces < pieces || starting_piece == -1) {
            if (pool->next == NULL) {
                pool->next = kmpool_create((size > DEFAULT_POOL_SIZE) ? size * 2 : DEFAULT_POOL_SIZE, pool->name);
                pool->next->ignore_mutex = pool->ignore_mutex;
            }
            
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

void* kmalloc(uint32_t size) {
    return _kmalloc(size, mem_pool0);
}

void* kzmalloc(uint32_t size) {
    void* pointer = _kmalloc(size, mem_pool0);
    memset(pointer, 0, size);

    return pointer;
}

void* _krealloc(void* space, uint32_t size, mem_pool_t* pool) {
    if (space == NULL) {
        if (size == 0)
            return NULL;

        return _kmalloc(size, pool);
    }
    if (size == 0) {
        _kfree(space, pool);
        return NULL;
    }
    mem_block_t* block = get_block_from_ext_addr(space);
    uint64_t oldsize   = block->pieces * POOL_PIECE_SIZE;

    void* new = _kmalloc(size, pool);
    memcpy(new, space, oldsize);
    
    _kfree(space, pool);

    return new;
}

void* krealloc(void* space, uint32_t size) {
    return _krealloc(space, size, mem_pool0);
}

void _kfree(void* space, mem_pool_t* pool) {
    mem_block_t* block = get_block_from_ext_addr(space);
    mem_pool_t* parent = find_parent(block, pool);

    mutex_acquire_yield(&(parent->mutex));

    set_pieces(parent, addr_to_piece(parent, block), block->pieces + 1, false);
    parent->free_pieces += block->pieces + 1;
    memset((void*) block, 0, (block->pieces + 1) * POOL_PIECE_SIZE);

    mutex_release(&(parent->mutex));
}

void kfree(void* space) {
    _kfree(space, mem_pool0);
}