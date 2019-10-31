#include "aex/io.h"
#include "aex/rcode.h"
#include "aex/kmem.h"

#include "dev/char.h"
#include "dev/dev.h"
#include "dev/input.h"

#include <stdio.h>

#include "ttyk.h"

int ttyk_open(int fd);
int ttyk_read(int fd, uint8_t* buffer, int len);
int ttyk_write(int fd, uint8_t* buffer, int len);
void ttyk_close(int fd);
long ttyk_ioctl(int fd, long code, void* mem);

struct dev_file_ops ttyk_ops = {
    .open  = ttyk_open,
    .read  = ttyk_read,
    .write = ttyk_write,
    .close = ttyk_close,
    .ioctl = ttyk_ioctl,
};
struct dev_char ttyk_dev = {
    .ops = &ttyk_ops,
    .internal_id = 0,
};

char* keymap;

size_t last = 0;

int ttyk_open(int internal_id) {
    return 0;
}

int ttyk_read(int internal_id, uint8_t* buffer, int len) {
    static bool mutex = false;

    while (mutex);
    mutex = true;
    
    int left = len;
    while (left > 0) {
        uint16_t k  = 0;
        uint8_t mod = 0;
        char c;

        last = input_kb_get((uint8_t*)&k, &mod, last);

        if (k == 0) {
            io_block();
            continue;
        }
        if (mod & INPUT_SHIFT_FLAG)
            k += 0x100;

        c = keymap[k];
        switch (c) {
            case '\r':
                c = '\n';
                break;
        }
        *buffer++ = c;
        --left;
    }
    mutex = false;
    return len;
}

int ttyk_write(int internal_id, uint8_t* buffer, int len) {
    static bool mutex = false;

    while (mutex);
    mutex = true;

    for (int i = 0; i < len; i++) 
        putchar(buffer[i]);

    mutex = false;
    return len;
}

void ttyk_close(int internal_id) {

}

void ttyk_init() {
    keymap = kmalloc(1024);
    input_fetch_keymap("us", keymap);

    dev_register_char("tty0", &ttyk_dev);
}

struct ttysize {
    uint16_t rows;
    uint16_t columns;
    uint16_t pixel_height;
    uint16_t pixel_width;
};

long ttyk_ioctl(int internal_id, long code, void* mem) {
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