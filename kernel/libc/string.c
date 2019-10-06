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

char* itoa(long num, char* str, int base) {
    long i = 0;
    bool isNegative = false;

    str[0] = 0;
    str[1] = 0;

    /* Handle 0 explicitely, otherwise empty string is printed for 0 */
    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    // In standard itoa(), negative numbers are handled only with
    // base 10. Otherwise numbers are considered unsigned.
    if (num < 0 && base == 10)
    {
        isNegative = true;
        num = -num;
    }

    // Process individual digits
    while (num != 0)
    {
        long rem = num % base;
        str[i++] = (rem > 9) ? (rem-10) + 'A' : rem + '0';
        num = num/base;
    }

    // If number is negative, append '-'
    if (isNegative)
        str[i++] = '-';

    str[i] = '\0'; // Append string terminator

    // Reverse the string
    reverse(str, i);

    return str;
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