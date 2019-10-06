#include "dev/char.h"
#include "dev/dev.h"

#include <stdio.h>

#include "ttyk.h"

int ttyk_open();
int ttyk_write(uint8_t* buffer, int len);
int ttyk_read(uint8_t* buffer, int len);
void ttyk_close();

struct dev_file_ops ttyk_ops = {
    .open  = ttyk_open,
    .write = ttyk_write,
    .read  = ttyk_read,
    .close = ttyk_close,
};
struct dev_char ttyk_dev = {
    .ops = &ttyk_ops,
};

int ttyk_open() {
    printf("ttyk was opened\n");
    return 0;
}

int ttyk_write(uint8_t* buffer, int len) {
    for (int i = 0; i < len; i++)
        putchar(buffer[i]);

    return len;
}

int ttyk_read(uint8_t* buffer, int len) {
    buffer = buffer;
    len = len;
    return 0;
}

void ttyk_close() {
    printf("ttyk was closed\n");
}

void ttyk_init() {
    dev_register_char("ttyk", &ttyk_dev);
}