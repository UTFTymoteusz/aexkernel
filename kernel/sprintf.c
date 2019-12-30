#include "aex/string.h"

#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/kernel.h"

static inline bool sprint(char* dst, const char* data, size_t length) {
	for (size_t i = 0; i < length; i++)
		dst[i] = data[i];

	return true;
}

int sprintf(char* dst, const char* restrict format, ...) {
	char itoa_buffer[128];
    char params[12];

    va_list parameters;
    va_start(parameters, format);

    int written = 0;

    char   pad_with = ' ';
    size_t pad_amnt  = 0;
    char width = '\0';

    while (*format != '\0') {
        size_t maxrem = INT_MAX - written;

        if (format[0] != '%' || format[1] == '%') {
            if (format[0] == '%')
                format++;

            size_t amount = 1;
            while (format[amount] && format[amount] != '%')
                amount++;

            if (maxrem < amount) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, format, amount))
                return -1;

            format  += amount;
            written += amount;
			dst     += amount;

            continue;
        }
        const char* format_begun_at = format++;

        pad_with = ' ';
        pad_amnt = 0;
        width = '\0';
        
        if (*format == '0') {
            pad_with = '0';
            ++format;
        }
        size_t offset = 0;
        
        params[offset] = '\0';
        for (; format[offset] >= '0' && format[offset] <= '9'; offset++)
            params[offset] = format[offset];

        params[offset] = '\0';

        format += offset;

        if (params[0] != '\0')
            pad_amnt = atoi(params);

        if (*format == 'c') {
            format++;
            char c = (char) va_arg(parameters, int);
            if (pad_amnt != 0)
                for (size_t i = 1; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, &c, sizeof(c)))
                return -1;

            written++;
			dst++;
        }
        else if (*format == 'x' || *format == 'X') {
            bool upper = *format == 'X';

            format++;

            if (width == 'l')
                ultoa((uint64_t) va_arg(parameters, uint64_t), itoa_buffer, 16);
            else if (width == 'h')
                uhtoa((uint16_t) va_arg(parameters, int), itoa_buffer, 16);
            else
                uitoa((uint32_t) va_arg(parameters, uint32_t), itoa_buffer, 16);

			unsigned int len = strlen(itoa_buffer);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (upper)
                for (size_t i = 0; i < len; i++)
                    itoa_buffer[i] = toupper(itoa_buffer[i]);
                    
            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, itoa_buffer, len))
                return -1;

            written += len;
			dst += len;
        }
        else if (*format == 'u') {
            format++;

            if (width == 'l')
                ltoa((uint64_t) va_arg(parameters, uint64_t), itoa_buffer, 10);
            else if (width == 'h')
                htoa((uint16_t) va_arg(parameters, unsigned int), itoa_buffer, 10);
            else
                itoa((uint32_t) va_arg(parameters, uint32_t), itoa_buffer, 10);

			unsigned int len = strlen(itoa_buffer);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, itoa_buffer, len))
                return -1;

            written += len;
			dst += len;
        }
        else if (*format == 'i') {
            format++;

            if (width == 'l')
                ltoa((int64_t) va_arg(parameters, int64_t), itoa_buffer, 10);
            else if (width == 'h')
                htoa((int16_t) va_arg(parameters, int), itoa_buffer, 10);
            else
                itoa((int32_t) va_arg(parameters, int32_t), itoa_buffer, 10);

			unsigned int len = strlen(itoa_buffer);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, itoa_buffer, len))
                return -1;

            written += len;
			dst += len;
        }
        else if (*format == 'p') {
            format++;
            ultoa((uint64_t) va_arg(parameters, void*), itoa_buffer, 16);

			unsigned int len = strlen(itoa_buffer);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written += len;
			dst += len;
        }
        else if (*format == 'b') {
            format++;
            ultoa((uint64_t) va_arg(parameters, uint64_t), itoa_buffer, 2);

			unsigned int len = strlen(itoa_buffer);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written += len;
			dst += len;
        }
        else if (*format == 's') {
            format++;
            const char* str = va_arg(parameters, const char*);

            size_t len = strlen(str);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    sprint(dst, &pad_with, 1);

            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, str, len))
                return -1;

            written += len;
			dst += len;
        }
        else {
            format = format_begun_at;
            size_t len = strlen(format);

            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!sprint(dst, format, len))
                return -1;

            written += len;
            format  += len;
			dst     += len;
        }
    }
    *dst = '\0';
    
    va_end(parameters);
    return written;
}