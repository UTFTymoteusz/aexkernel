#include <string.h>
#include <stdio.h>

#include "aex/kmem.h"
#include "aex/time.h"

#include "aex/cbufm.h"

int cbufm_create(cbufm_t* cbufm, size_t size) {
    memset(cbufm, 0, sizeof(cbufm_t));
    cbufm->size   = size;
    cbufm->buffer = kmalloc(size);

    return 0;
}

size_t cbufm_read(cbufm_t* cbufm, uint8_t* buffer, size_t start, size_t len) {
    //printf("start %i\n", start);
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
    //printf("end %i\n", start);
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

size_t cbufm_sync(cbufm_t* cbufm) {
    return cbufm->write_ptr;
}

size_t cbufm_available(cbufm_t* cbufm, size_t start) {
    if (start == cbufm->write_ptr)
        return 0;

    if (start > cbufm->write_ptr) {
        size_t avail = (cbufm->size - start) + cbufm->write_ptr;

        //printf("avail _l %i\n", avail);
        return avail;
    }
    else {
        //printf("avail %i\n", cbufm->write_ptr - start);
        return cbufm->write_ptr - start;
    }
}