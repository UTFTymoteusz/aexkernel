#include <dev/tty.h>

int putchar(int ic) {
    tty_putchar((char)ic);
    return ic;
}