#include "aex/string.h"

#include <limits.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/kernel.h"

static inline int snprintf_common(char* dst, size_t* remaining, char* buffer, char pad_char, size_t pad_len) {
    int len = 0;

    if (pad_len != 0) {
        int len = pad_len - strlen(buffer);

        for (; len > 0 ; len--) {
            if (*remaining == 0)
                return len;

            (*remaining)--;

            *dst++ = pad_char;
            len++;
        }
    }
    while (*buffer) {
        if (*remaining == 0)
            return len;

        (*remaining)--;

        *dst++ = *buffer;
        buffer++;
        len++;
    }
    return len;
}

int snprintf(char* dst, size_t size, char* format, ...) {
    if (size == 0)
        return 0;

    size--;
    size_t org_len = size;

    va_list params;
    va_start(params, format);

    char type = '\0';
    char leng = '\0';

    char   pad_with = ' ';
    size_t pad_len;

    char buffer[72];
    size_t blen;

    while (*format != '\0') {
        type = '\0';
        leng = '\0';
        pad_len = 0;
        
        if (*format == '%') {
            pad_with = ' ';
            format++;

            if (*format == '0') {
                pad_with = *format;
                format++;
            }

            blen = 0;

            while (*format >= '0' && *format <= '9')
                buffer[blen++] = *format++;
            buffer[blen] = '\0';
            pad_len = atoi(buffer);

            if (*format == 'l' || *format == 'h') {
                leng = *format;
                format++;
            }
            type = *format;

            blen =  0;
        }

        switch (type) {
            case '$':
                format++;
                if (*format != '{')
                    break;
                    
                format++;
                while (*format != '}' && *format != '\0') {
                    buffer[blen] = *format;
                    blen += 1;

                    format++;
                }
                buffer[blen] = '\0';
                tty_set_color_ansi(0, atoi(buffer));
                break;
            case 'i':
                if (leng == 'h')
                    htoa((int16_t) va_arg(params, int), buffer, 10);
                else if (leng == 'l')
                    ltoa((int64_t) va_arg(params, int64_t), buffer, 10);
                else
                    itoa((int32_t) va_arg(params, int32_t), buffer, 10);

                dst += snprintf_common(dst, &size, buffer, pad_with, pad_len);
                break;
            case 'u':
                if (leng == 'h')
                    uhtoa((uint16_t) va_arg(params, int), buffer, 10);
                else if (leng == 'l')
                    ultoa((uint64_t) va_arg(params, uint64_t), buffer, 10);
                else
                    uitoa((uint32_t) va_arg(params, uint32_t), buffer, 10);

                dst += snprintf_common(dst, &size, buffer, pad_with, pad_len);
                break;
            case 'X':
            case 'x':
                if (leng == 'h')
                    uhtoa((uint16_t) va_arg(params, int), buffer, 16);
                else if (leng == 'l')
                    ultoa((uint64_t) va_arg(params, uint64_t), buffer, 16);
                else
                    uitoa((uint32_t) va_arg(params, uint32_t), buffer, 16);

                if (type == 'X' || type == 'P') {
                    blen = strlen(buffer);
                    for (size_t i = 0; i < blen; i++)
                        buffer[i] = toupper(buffer[i]);
                }
                dst += snprintf_common(dst, &size, buffer, pad_with, pad_len);
                break;
            case 'p':
            case 'P':
                pad_with = '0';
                ultoa((size_t) va_arg(params, size_t), buffer, 16);

                if (type == 'X' || type == 'P') {
                    blen = strlen(buffer);
                    for (size_t i = 0; i < blen; i++)
                        buffer[i] = toupper(buffer[i]);
                }
                dst += snprintf_common(dst, &size, buffer, pad_with, pad_len);
                break;
            case 's':
                dst += snprintf_common(dst, &size, (char*) va_arg(params, char*), pad_with, pad_len);
                break;
            default:
                if (size > 0) {
                    size--;
                    *dst = *format;
                    dst++;
                }
                break;
        }
        format++;
    }
    va_end(params);

    *dst = '\0';
    return org_len - size;
}