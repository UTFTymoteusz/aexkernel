#pragma once

// I need to think about a better name
struct ilocation {
    uint64_t blocks[16];
    struct ilocation* next;
};
typedef struct ilocation ilocation_t;