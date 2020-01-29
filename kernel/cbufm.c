#include "aex/io.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/string.h"
#include "aex/time.h"

#include "aex/cbufm.h"

int cbufm_create(cbufm_t* cbufm, size_t size) {
    memset(cbufm, 0, sizeof(cbufm_t));
    cbufm->size   = size;
    cbufm->buffer = kmalloc(size);
    cbufm->mutex.val  = 0;
    cbufm->mutex.name = NULL;

    io_create_bqueue(&(cbufm->bqueue));

    return 0;
}

size_t cbufm_read(cbufm_t* cbufm, uint8_t* buffer, size_t start, size_t len) {
    size_t possible, w_off;
    size_t size = cbufm->size;

    while (len > 0) {
        mutex_wait(&(cbufm->mutex));

        w_off = cbufm->write_ptr;
        if (w_off < start)
            w_off += size;

        possible = cbufm->size - start;

        if (possible > (w_off - start))
            possible = w_off - start;

        if (possible > len)
            possible = len;

        if (possible == 0) {
            io_block(&(cbufm->bqueue));
            continue;
        }
        memcpy(buffer, cbufm->buffer + start, possible);

        len -= possible;
        buffer += possible;

        start += possible;
        if (start == cbufm->size)
            start = 0;
    }
    return start;
}

size_t cbufm_write(cbufm_t* cbufm, uint8_t* buffer, size_t len) {
    size_t amnt;
    mutex_acquire(&(cbufm->mutex));

    while (len > 0) {
        amnt = cbufm->size - cbufm->write_ptr;
        if (amnt > len)
            amnt = len;

        len -= amnt;

        memcpy(cbufm->buffer + cbufm->write_ptr, buffer, amnt);
        buffer += amnt;

        cbufm->write_ptr += amnt;

        if (cbufm->write_ptr == cbufm->size)
            cbufm->write_ptr = 0;
    }
    io_unblockall(&(cbufm->bqueue));
    mutex_release(&(cbufm->mutex));

    return cbufm->write_ptr;
}

size_t cbufm_sync(cbufm_t* cbufm) {
    return cbufm->write_ptr;
}

size_t cbufm_available(cbufm_t* cbufm, size_t start) {
    mutex_wait(&(cbufm->mutex));

    if (start == cbufm->write_ptr)
        return 0;

    if (start > cbufm->write_ptr)
        return (cbufm->size - start) + cbufm->write_ptr;

    return cbufm->write_ptr - start;
}

void cbufm_wait(cbufm_t* cbufm, size_t start) {
    while (true) {
        if (cbufm_available(cbufm, start) == 0) {
            io_block(&(cbufm->bqueue));
            continue;
        }
        break;
    }
}