#pragma once

#include <stddef.h>

#include <string/memcmp.c>
#include <string/memcpy.c>
#include <string/memmove.c>
#include <string/memset.c>
#include <string/strcmp.c>
#include <string/strcpy.c>
#include <string/strlen.c>
#include <string/itoa.c>

int   memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);

int    strcmp(char*, char*);
size_t strlen(const char*);

char* itoa(long, char*, int);