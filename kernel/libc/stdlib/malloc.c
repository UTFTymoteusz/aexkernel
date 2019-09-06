#pragma once

#include <stdio.h>
#include <string.h>
#include <kernel/mem.h>
#include <kernel/sys.h>

unsigned long long allocated = 0;

typedef struct block {
    size_t size;
    char free;
    struct block* next;
} block;

//void* malloc(size_t size) {
//
//    char stringbuffer[32];
//    printf("malloc()ing %s bytes", itoa(size, stringbuffer, 10));
//
//    size_t required = size + sizeof(block);
//
//    void* ptr;
//    
//    if (allocated == 0) {
//        ptr = mem_frame_alloc(memory_frame_get_free());
//        allocated += 0x1000;
//    }
//
//    return ptr;
//}