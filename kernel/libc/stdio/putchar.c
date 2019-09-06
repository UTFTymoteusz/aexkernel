#pragma once

#include <dev/tty.h>

int putchar(int ic) {
    
    char c = (char) ic;
    tty_putchar(c);

    return ic;
}