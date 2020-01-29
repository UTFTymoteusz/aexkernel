#include "aex/string.h"

#include "aex/dev/tty.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "aex/kernel.h"

static inline bool puts(const char* data, size_t length) {
    const uint8_t* bytes = (const uint8_t*) data;

    for (size_t i = 0; i < length; i++)
        tty_putchar(bytes[i]);

    tty_putchar('\n');
    return true;
}

int putsk(const char* str) {
    int len = strlen(str);
    if (!puts(str, len))
        return 0;

    return len + 1;
}