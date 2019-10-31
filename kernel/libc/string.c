#include "stdbool.h"
#include "stddef.h"
#include "stdint.h"

#include "string.h"

void reverse(char str[], int length) {
    int start = 0;
    int end = length -1;
    char c;

    while (start < end)
    {
        c = str[start];
        str[start] = str[end];
        str[end] = c;

        start++;
        end--;
    }
}

char* itoa(long value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    // Check for supported base.
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    // Set '-' for negative decimals.
    if (value < 0 && base == 10)
        *ptr++ = '-';

    // Remember where the numbers start.
    low = ptr;
    // The actual conversion.
    do {
        // Modulo is negative for negative value. This trick makes abs() unnecessary.
        *ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz"[35 + value % base];
        value /= base;
    } 
    while (value);
    // Terminating the string.
    *ptr-- = '\0';
    // Invert the numbers.
    while (low < ptr) {
        char tmp = *low;

        *low++ = *ptr;
        *ptr-- = tmp;
    }
    return rc;
}

int memcmp(const void* aptr, const void* bptr, size_t size) {
	const unsigned char* a = (const unsigned char*) aptr;
	const unsigned char* b = (const unsigned char*) bptr;

	for (size_t i = 0; i < size; i++) {
		if (a[i] < b[i])
			return -1;
		else if (b[i] < a[i])
			return 1;
	}
	return 0;
}

void* memcpy(void* restrict dstptr, const void* restrict srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;

	for (size_t i = 0; i < size; i++)
		dst[i] = src[i];

	return dstptr;
}

void* memmove(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;

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
	unsigned char* buf = (unsigned char*) bufptr;
	for (size_t i = 0; i < size; i++)
		buf[i] = (unsigned char) value;

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