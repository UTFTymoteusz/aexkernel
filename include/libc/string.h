#pragma once

#include <stddef.h>

int   memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);

int    strcmp(char* left, char* right);
char*  strcpy(char* dst, char* src);
size_t strlen(const char* str);

char* itoa(long, char*, int);
long  atoi(char*);