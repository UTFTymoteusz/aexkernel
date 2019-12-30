#include "aex/string.h"
#include "aex/time.h"

#include "aex/dev/tty.h"

#include <limits.h>
#include <stdarg.h>

#include "aex/kernel.h"

uint32_t ttyk_flags;
bool printk_print_newline = true;

static inline bool print(const char* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*) data;

    for (size_t i = 0; i < length; i++)
        tty_putchar(bytes[i]);

    return true;
}

uint32_t get_printk_flags() {
    return ttyk_flags;
}

void set_printk_flags(uint32_t flags) {
    ttyk_flags = flags;
}

int printk(const char* format, ...) {
    bool spec = *format == '^';
    if (spec)
        format++;

    bool skip_padding = false;

    if (spec && printk_print_newline) {
        switch (*format) {
            case '1':
                tty_set_color_ansi(90);
                print(" [  ", 4);
                tty_set_color_ansi(97);

                tty_set_color_ansi(92);
                print("OK", 2);

                tty_set_color_ansi(90);
                print("  ] ", 4);
                tty_set_color_ansi(97);

                skip_padding = true;
                break;
            case '2':
                tty_set_color_ansi(90);
                print(" [ ", 3);
    
                tty_set_color_ansi(93);
                print("WARN", 4);

                tty_set_color_ansi(90);
                print(" ] ", 3);
                tty_set_color_ansi(97);

                skip_padding = true;
                break;
            case '4':
                tty_set_color_ansi(90);
                print(" [ ", 3);
    
                tty_set_color_ansi(94);
                print("INIT", 4);

                tty_set_color_ansi(90);
                print(" ] ", 3);
                tty_set_color_ansi(97);

                skip_padding = true;
                break;
        }
        format++;
    }

    if (printk_print_newline) {
        if (!skip_padding)
            print("  ", 2);

        printk_print_newline = false;
    }
    
    /*if ((ttyk_flags & PRINTK_TIME) > 0 && printk_print_newline) {
        if (print_omit_ext)
            print("              ", 14);
        else {
            tty_set_color_ansi(90);
            print(" [", 2);
            tty_set_color_ansi(97);

            char time[12];
            gcvt(get_ms_passed() / 1000, 3, time);
            if (strlen(time) < 8)
                print("        ", 8 - strlen(time));

            print(time, strlen(time));

            tty_set_color_ansi(90);
            print(" ]  ", 4);
            tty_set_color_ansi(97);
        }
            
        printk_print_newline = false;
    }*/
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
            if (!print(format, amount))
                return -1;

            format  += amount;
            written += amount;

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

        if (*format == 'l' || *format == 'h')
            width = *format++;

        if (params[0] != '\0')
            pad_amnt = atoi(params);

        if (*format == 'c') {
            format++;
            char c = (char) va_arg(parameters, int);
            if (pad_amnt != 0)
                for (size_t i = 1; i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(&c, sizeof(c)))
                return -1;

            written++;
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

            if (pad_amnt != 0)
                for (size_t i = strlen(itoa_buffer); i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (upper)
                for (size_t i = 0; i < strlen(itoa_buffer); i++)
                    itoa_buffer[i] = toupper(itoa_buffer[i]);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written++;
        }
        else if (*format == 'u') {
            format++;

            if (width == 'l')
                ultoa((uint64_t) va_arg(parameters, uint64_t), itoa_buffer, 10);
            else if (width == 'h')
                uhtoa((uint16_t) va_arg(parameters, unsigned int), itoa_buffer, 10);
            else
                uitoa((uint32_t) va_arg(parameters, uint32_t), itoa_buffer, 10);

            if (pad_amnt != 0)
                for (size_t i = strlen(itoa_buffer); i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written++;
        }
        else if (*format == 'i') {
            format++;

            if (width == 'l')
                ltoa((int64_t) va_arg(parameters, int64_t), itoa_buffer, 10);
            else if (width == 'h')
                htoa((int16_t) va_arg(parameters, int), itoa_buffer, 10);
            else
                itoa((int32_t) va_arg(parameters, int32_t), itoa_buffer, 10);

            if (pad_amnt != 0)
                for (size_t i = strlen(itoa_buffer); i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written++;
        }
        else if (*format == 'p') {
            format++;
            ultoa((uint64_t) va_arg(parameters, void*), itoa_buffer, 16);

            if (pad_amnt != 0)
                for (size_t i = strlen(itoa_buffer); i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written++;
        }
        else if (*format == 'b') {
            format++;
            ultoa((uint64_t) va_arg(parameters, uint64_t), itoa_buffer, 2);

            if (pad_amnt != 0)
                for (size_t i = strlen(itoa_buffer); i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (!maxrem) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(itoa_buffer, strlen(itoa_buffer)))
                return -1;

            written++;
        }
        else if (*format == 's') {
            format++;
            const char* str = va_arg(parameters, const char*);

            size_t len = strlen(str);
            if (pad_amnt != 0)
                for (size_t i = len; i < pad_amnt; i++)
                    print(&pad_with, 1);

            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(str, len))
                return -1;

            written += len;
        }
        else {
            format = format_begun_at;
            size_t len = strlen(format);

            if (maxrem < len) {
                // TODO: Set errno to EOVERFLOW.
                return -1;
            }
            if (!print(format, len))
                return -1;

            written += len;
            format += len;
        }
    }
    format--;
    if (*format == '\n')
        printk_print_newline = true;
    
    va_end(parameters);
    return written;
}