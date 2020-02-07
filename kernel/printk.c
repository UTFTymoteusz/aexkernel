#include "aex/string.h"
#include "aex/time.h"

#include "aex/dev/tty.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>

#include "aex/kernel.h"

static inline void printk_common(char* buffer, char pad_char, size_t pad_len) {
    if (pad_len != 0) {
        int len = pad_len - strlen(buffer);

        for (; len > 0 ; len--)
            tty_putchar(0, pad_char);
    }
    tty_write(0, buffer);
}

void printk(char* format, ...) {
    static bool newline_encountered = true;
    
    bool spec = *format == '^';
    if (spec)
        format++;

    bool skip_padding = false;

    if (spec && newline_encountered) {
        switch (*format) {
            case '1':
                tty_set_color_ansi(0, 90);
                tty_write(0, " [  ");
                tty_set_color_ansi(0, 97);

                tty_set_color_ansi(0, 92);
                tty_write(0, "OK");

                tty_set_color_ansi(0, 90);
                tty_write(0, "  ] ");
                tty_set_color_ansi(0, 97);

                skip_padding = true;
                break;
            case '2':
                tty_set_color_ansi(0, 90);
                tty_write(0, " [ ");
    
                tty_set_color_ansi(0, 93);
                tty_write(0, "WARN");

                tty_set_color_ansi(0, 90);
                tty_write(0, " ] ");
                tty_set_color_ansi(0, 97);

                skip_padding = true;
                break;
            case '4':
                tty_set_color_ansi(0, 90);
                tty_write(0, " [ ");
    
                tty_set_color_ansi(0, 94);
                tty_write(0, "INIT");

                tty_set_color_ansi(0, 90);
                tty_write(0, " ] ");
                tty_set_color_ansi(0, 97);

                skip_padding = true;
                break;
        }
        format++;
    }

    if (newline_encountered) {
        if (!skip_padding)
            tty_write(0, "  ");

        newline_encountered = false;
    }
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
        else if (*format == '\n')
            newline_encountered = true;

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

                printk_common(buffer, pad_with, pad_len);
                break;
            case 'u':
                if (leng == 'h')
                    uhtoa((uint16_t) va_arg(params, int), buffer, 10);
                else if (leng == 'l')
                    ultoa((uint64_t) va_arg(params, uint64_t), buffer, 10);
                else
                    uitoa((uint32_t) va_arg(params, uint32_t), buffer, 10);

                printk_common(buffer, pad_with, pad_len);
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
                printk_common(buffer, pad_with, pad_len);
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
                printk_common(buffer, pad_with, pad_len);
                break;
            case 's':
                printk_common((char*) va_arg(params, char*), pad_with, pad_len);
                break;
            case 'c':
                buffer[0] = (char) va_arg(params, int);
                buffer[1] = '\0';
                printk_common(buffer, pad_with, pad_len);
                break;
            default:
                tty_putchar(0, *format);
                break;
        }
        format++;
    }
    va_end(params);
}