#include <string.h>
#include <stdio.h>

#include "aex/kmem.h"
#include "aex/time.h"

#include "aex/cbufm.h"

int cbufm_create(cbufm_t* cbufm, size_t size) {
    memset(cbufm, 0, sizeof(cbufm_t));
    cbufm->size   = size;
    cbufm->buffer = kmalloc(size);
}

int cbufm_read(cbufm_t* cbufm, uint8_t* buffer, size_t start, size_t len) {
    size_t amnt;
    while (len > 0) {
        amnt = cbufm->size - start;
        if (amnt > len)
            amnt = len;

        //printf("can afford to write %i, len %i\n", amnt, len);
        len -= amnt;

        memcpy(buffer, cbufm->buffer + start, amnt);
        buffer += amnt;

        start += amnt;

        if (start == cbufm->size)
            start = 0;
    }
    return start;
}

size_t cbufm_write(cbufm_t* cbufm, uint8_t* buffer, size_t len) {
    size_t amnt;
    while (len > 0) {
        amnt = cbufm->size - cbufm->write_ptr;
        if (amnt > len)
            amnt = len;

        //printf("can afford to write %i, len %i\n", amnt, len);
        len -= amnt;

        memcpy(cbufm->buffer + cbufm->write_ptr, buffer, amnt);
        buffer += amnt;

        cbufm->write_ptr += amnt;

        if (cbufm->write_ptr == cbufm->size)
            cbufm->write_ptr = 0;
    }
    return cbufm->write_ptr;
}