#pragma once

#include <stddef.h>

int   memcmp(const void*, const void*, size_t);
void* memcpy(void* __restrict, const void* __restrict, size_t);
void* memmove(void*, const void*, size_t);
void* memset(void*, int, size_t);

int    strcmp(char* left, char* right);
char*  strcpy(char* dst, char* src);
size_t strlen(const char* str);

char* itoa(int, char*, int);
char* ltoa(long, char*, int);
char* htoa(short, char*, int);

char* uitoa(unsigned int, char*, int);
char* ultoa(unsigned long, char*, int);
char* uhtoa(unsigned short, char*, int);

int   atoi(char* str);
long  atol(char* str);
short atoh(char* str);

unsigned int   atoui(char* str);
unsigned long  atoul(char* str);
unsigned short atouh(char* str);

char* gcvt(double, int, char*);

int toupper(int c);
int tolower(int c);