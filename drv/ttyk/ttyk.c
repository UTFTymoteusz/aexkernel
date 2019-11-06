#include "aex/io.h"
#include "aex/kmem.h"
#include "aex/mutex.h"
#include "aex/rcode.h"

#include "dev/char.h"
#include "dev/dev.h"
#include "dev/input.h"
#include "dev/tty.h"

#include <stdio.h>
#include <string.h>

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
    static mutex_t mutex = 0;

    //mutex_acquire(&mutex);
    
    int left = len;
    while (left > 0) {
        uint16_t k  = 0;
        uint8_t mod = 0;
        char c;

        input_kb_wait(last);
        last = input_kb_get((uint8_t*)&k, &mod, last);

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
    //mutex_release(&mutex);
    return len;
}

#include "dev/cpu.h"

static inline void interpret_ansi(char* buffer, size_t offset) {
    char start, final;
    start = buffer[0];
    final = buffer[offset];

    uint16_t args[6];
    char arg_buf[8];

    uint16_t ptr = 0;
    uint16_t arg = 0;

    for (int i = 0; i < 6; i++)
        args[i] = 0;

    char c;
    for (size_t i = 1; i <= offset; i++) {
        c = buffer[i];
        if ((c >= ':' && c <= '?') || i == offset) {
            if (ptr != 0) {
                arg_buf[ptr] = '\0';
                args[arg++]  = atoi(arg_buf);
                ptr = 0;
            }
            continue;
        }
        arg_buf[ptr++] = c;
    }
    switch (final) {
        case 'm':
            tty_set_color_ansi(args[0]);
            break;
    }
}

static inline void tty_write_internal(char c) {
    static size_t state = 0;
    static size_t offset = 0;
    static char buffer[24];

    switch (state) {
        case 0:
            if (c == 27) {
                state = 1;
                break;
            }
            putchar(c);
            break;
        case 1:
            if (c >= '@' && c <= '_') {
                state = 2;
                buffer[offset++] = c;
                break;
            }
            state = 0;
            break;
        case 2:
            buffer[offset++] = c;
            if (c >= '@' && c <= '~') {
                state = 0;
                interpret_ansi(buffer, --offset);
                offset = 0;
            }
            else if (offset > 22) {
                state  = 0;
                offset = 0;
            }
            break;
    }
}

int ttyk_write(int internal_id, uint8_t* buffer, int len) {
    static mutex_t mutex = 0;

    mutex_acquire(&mutex);

    for (int i = 0; i < len; i++)
        tty_write_internal(buffer[i]);

    mutex_release(&mutex);
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