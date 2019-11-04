#include <string.h>
#include <stdio.h>

#include "aex/kmem.h"
#include "aex/time.h"

#include "aex/cbuf.h"

int cbuf_create(cbuf_t* cbuf, size_t size) {
    memset(cbuf, 0, sizeof(cbuf_t));
    cbuf->size   = size;
    cbuf->buffer = kmalloc(size);

    return 0;
}

size_t cbuf_read(cbuf_t* cbuf, uint8_t* buffer, size_t len) {
    size_t possible, wdoff;
    size_t size = cbuf->size;

    //printf("read: %i\n", len);

    while (len > 0) {
        mutex_acquire(&(cbuf->mutex));

        wdoff = cbuf->write_ptr;
        if (wdoff < cbuf->read_ptr)
            wdoff += size;

        possible = cbuf->size - cbuf->read_ptr;

        if (possible > (wdoff - cbuf->read_ptr))
            possible = wdoff - cbuf->read_ptr;

        if (possible > len)
            possible = len;

        //printf("rposs: %i, start at: %i, len: %i\n", possible, cbuf->read_ptr, len);
        //printf("a: %i, b: %i\n", wdoff, cbuf->write_ptr);
        if (possible == 0) {
            mutex_release(&(cbuf->mutex));
            yield();
            continue;
        }
        memcpy(buffer, cbuf->buffer + cbuf->read_ptr, possible);

        len -= possible;
        buffer += possible;

        cbuf->read_ptr += possible;
        if (cbuf->read_ptr == cbuf->size)
            cbuf->read_ptr = 0;

        mutex_release(&(cbuf->mutex));
    }
    //printf("rptr: %i\n", cbuf->read_ptr);
    //printf("end %i\n", start);
    mutex_release(&(cbuf->mutex));
    return cbuf->read_ptr;
}

size_t cbuf_write(cbuf_t* cbuf, uint8_t* buffer, size_t len) {
    size_t possible, rdoff;
    size_t size = cbuf->size;

    //printf("write: %i\n", len);

    while (len > 0) {
        mutex_acquire(&(cbuf->mutex));

        rdoff = cbuf->read_ptr;
        if (rdoff <= cbuf->write_ptr)
            rdoff += size;

        possible = cbuf->size - cbuf->write_ptr;

        if (possible > ((rdoff - 1) - cbuf->write_ptr))
            possible = (rdoff - 1) - cbuf->write_ptr;

        if (possible > len)
            possible = len;

        //printf("wposs: %i, start at: %i\n", possible, cbuf->write_ptr);
        //printf("a: %i, b: %i\n", rdoff, cbuf->read_ptr);
        if (possible == 0) {
            mutex_release(&(cbuf->mutex));
            yield();
            continue;
        }
        memcpy(cbuf->buffer + cbuf->write_ptr, buffer, possible);

        len -= possible;
        buffer += possible;

        cbuf->write_ptr += possible;
        if (cbuf->write_ptr == cbuf->size)
            cbuf->write_ptr = 0;

        mutex_release(&(cbuf->mutex));
    }
    //printf("wptr: %i\n", cbuf->write_ptr);
    mutex_release(&(cbuf->mutex));
    return cbuf->write_ptr;
}

size_t cbuf_available(cbuf_t* cbuf) {
    size_t rp = cbuf->read_ptr;
    size_t wp = cbuf->write_ptr;
    size_t sz = cbuf->size;

    if (rp == wp)
        return 0;

    if (rp > wp)
        return (sz - rp) + wp;
    else
        return wp - rp;
}