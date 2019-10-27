#include "aex/rcode.h"

#include "dev/char.h"
#include "dev/dev.h"

#include <stdio.h>

#include "ttyk.h"

int ttyk_open();
int ttyk_read(uint8_t* buffer, int len);
int ttyk_write(uint8_t* buffer, int len);
void ttyk_close();
long ttyk_ioctl(long code, void* mem);

struct dev_file_ops ttyk_ops = {
    .open  = ttyk_open,
    .read  = ttyk_read,
    .write = ttyk_write,
    .close = ttyk_close,
    .ioctl = ttyk_ioctl,
};
struct dev_char ttyk_dev = {
    .ops = &ttyk_ops,
};

int ttyk_open() {
    return 0;
}

int ttyk_read(uint8_t* buffer, int len) {
    buffer = buffer;
    len = len;
    return 0;
}

int ttyk_write(uint8_t* buffer, int len) {
    static bool mutex = false;

    while (mutex);
    mutex = true;

    for (int i = 0; i < len; i++)
        putchar(buffer[i]);

    mutex = false;
    return len;
}

void ttyk_close() {

}

void ttyk_init() {
    dev_register_char("ttyk", &ttyk_dev);
}

struct ttysize {
    uint16_t rows;
    uint16_t columns;
    uint16_t pixel_height;
    uint16_t pixel_width;
};

long ttyk_ioctl(long code, void* mem) {
    switch (code) {
        case 0xA1:
            ; // Stupid C
            struct ttysize* ttysz = mem;

            ttysz->rows    = 80;
            ttysz->columns = 25;
            ttysz->pixel_height = 400;
            ttysz->pixel_width  = 720;

            printf("\nsize\n\n");

            return 0;
        default:
            return DEV_ERR_NOT_SUPPORTED;
    }
}