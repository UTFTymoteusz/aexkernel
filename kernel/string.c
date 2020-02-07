#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

#include "aex/string.h"

#define NTOA_DICTIONARY "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"
// this is the most autistic thing I've ever done, but it works ¯\_(ツ)_/¯
#define NTOA_COMMON \
    char* rc;  \
    char* ptr; \
    char* low; \
    \
    if (base < 2 || base > 36) { \
        *str = '\0'; \
        return str;  \
    } \
    rc = ptr = str; \
    if (value < 0 && base == 10) \
        *ptr++ = '-'; \
    \
    low = ptr;      \
    do { \
        *ptr++ = NTOA_DICTIONARY[35 + value % base]; \
        value /= base; \
    } \
    while (value); \
    \
    *ptr-- = '\0'; \
    \
    while (low < ptr) { \
        char tmp = *low; \
        \
        *low++ = *ptr; \
        *ptr-- = tmp;  \
    } \
    return rc;

#define UNTOA_COMMON \
    char* rc;  \
    char* ptr; \
    char* low; \
    \
    if (base < 2 || base > 36) { \
        *str = '\0'; \
        return str;  \
    } \
    rc = ptr = str; \
    \
    low = ptr;      \
    do { \
        *ptr++ = NTOA_DICTIONARY[35 + value % base]; \
        value /= base; \
    } \
    while (value); \
    \
    *ptr-- = '\0'; \
    \
    while (low < ptr) { \
        char tmp = *low; \
        \
        *low++ = *ptr; \
        *ptr-- = tmp;  \
    } \
    return rc;

#define ATON_COMMON(x) \
    x   res  = 0; \
    int sign = 1; \
    int i    = 0; \
    \
    if (str[0] == '-') { \
        sign = -1; \
        ++i;       \
    } \
    \
    for (; str[i] != '\0'; ++i) \
        res = res * 10 + str[i] - '0'; \
    \
    return sign * res; 

#define UATON_COMMON(x) \
    x   res  = 0; \
    int i    = 0; \
    \
    for (; str[i] != '\0'; ++i) \
        res = res * 10 + str[i] - '0'; \
    \
    return res; 

char* itoa(int value, char* str, int base) {
    NTOA_COMMON
}
char* ltoa(long value, char* str, int base) {
    NTOA_COMMON
}
char* htoa(short value, char* str, int base) {
    NTOA_COMMON
}

char* uitoa(unsigned int value, char* str, int base) {
    UNTOA_COMMON
}
char* ultoa(unsigned long value, char* str, int base) {
    UNTOA_COMMON
}
char* uhtoa(unsigned short value, char* str, int base) {
    UNTOA_COMMON
}

int atoi(char* str) { 
    ATON_COMMON(int)
}
long atol(char* str) { 
    ATON_COMMON(long)
}
short atoh(char* str) { 
    ATON_COMMON(short)
}

unsigned int atoui(char* str) { 
    UATON_COMMON(unsigned int)
}
unsigned long atoul(char* str) { 
    UATON_COMMON(unsigned long)
}
unsigned short atouh(char* str) { 
    UATON_COMMON(unsigned short)
}

char* gcvt(double val, int digits, char* buf) {
    char* start = buf;

    double div = 1000000000000000000; // idk
    bool started = false;
    for (int i = 0; i < 18; i++) {
        div /= 10;
        if ((val / div) < 1 && !started)
            continue;

        started = true;
        *buf = '0' + ((int) (val / div) % 10);
        ++buf;
    }
    if (!started)
        *buf++ = '0';
        
    double after_dec = val - (int) val;
    if (after_dec == 0) {
        *buf = '\0';
        return start;
    }
    *buf = '.';
    buf++;

    div = 1;
    for (int i = 0; i < digits; i++) {
        div *= 10;

        *buf = '0' + ((int) (val * div) % 10);
        ++buf;
    }
    *buf = '\0';
    return start;
}

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const uint8_t* a = (const uint8_t*) aptr;
	const uint8_t* b = (const uint8_t*) bptr;

	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
	uint8_t* dst = (uint8_t*) dstptr;
	const uint8_t* src = (const uint8_t*) srcptr;

	for (size_t i = 0; i < size; i++)
		dst[i] = src[i];

	return dstptr;
}

void* memmove(void* dstptr, const void* srcptr, size_t size) {
	uint8_t* dst = (uint8_t*) dstptr;
	const uint8_t* src = (const uint8_t*) srcptr;

	if (dst < src) {
		for (size_t i = 0; i < size; i++)
			dst[i] = src[i];
	}
    else {
		for (size_t i = size; i != 0; i--)
			dst[i-1] = src[i-1];
	}
	return dstptr;
}

void* memset(void* bufptr, int value, size_t size) {
	uint8_t* buf = (uint8_t*) bufptr;
	for (size_t i = 0; i < size; i++)
		buf[i] = (uint8_t) value;

	return bufptr;
}

int strcmp(char* left, char* right) {
    int i = 0;

    while (true) {
        if (left[i] == right[i]) {
            if (left[i] == '\0')
                return 0;
        }
        else
            return left[i] < right[i] ? -1 : 1;

        ++i;
    }
    return 0;
}

char* strcpy(char* dst, char* src) {
    int i = 0;
    
    while (src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
    return dst;
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len])
		len++;

	return len;
}

int toupper(int c) {
    return c >= 'a' && c <= 'z' ? c - 0x20 : c;
}

int tolower(int c) {
    return c >= 'A' && c <= 'Z' ? c + 0x20 : c;
}

char* strchrnul(char* str, int c) {
    while (true) {
        if (*str == c || *str == '\0')
            return str;
            
        str++;
    }
}