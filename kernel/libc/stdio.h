#pragma once

#define EOF (-1)

#include <stdio/putchar.c>
#include <stdio/printf.c>
#include <stdio/sprintf.c>
#include <stdio/puts.c>

int printf(const char* __restrict, ...);
int sprintf(char* buffer, const char* __restrict, ...);
int putchar(int);
int puts(const char*);