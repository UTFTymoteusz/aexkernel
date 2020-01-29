#include "aex/io.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/string.h"
#include "aex/time.h"

#include "aex/cbuf.h"

int cbuf_create(cbuf_t* cbuf, size_t size) {
    memset(cbuf, 0, sizeof(cbuf_t));
    cbuf->size   = size;
    cbuf->buffer = kmalloc(size);
    cbuf->mutex.val  = 0;
    cbuf->mutex.name = NULL;
    
    io_create_bqueue(&(cbuf->bqueue));

    return 0;
}

size_t cbuf_read(cbuf_t* cbuf, uint8_t* buffer, size_t len) {
    size_t possible, w_off;
    size_t size = cbuf->size;

    while (len > 0) {
        mutex_acquire(&(cbuf->mutex));

        w_off = cbuf->write_ptr;
        if (w_off < cbuf->read_ptr)
            w_off += size;

        possible = cbuf->size - cbuf->read_ptr;

        if (possible > (w_off - cbuf->read_ptr))
            possible = w_off - cbuf->read_ptr;

        if (possible > len)
            possible = len;

        if (possible == 0) {
            mutex_release(&(cbuf->mutex));
            io_block(&(cbuf->bqueue));

            continue;
        }
        memcpy(buffer, cbuf->buffer + cbuf->read_ptr, possible);

        len -= possible;
        buffer += possible;

        cbuf->read_ptr += possible;
        if (cbuf->read_ptr == cbuf->size)
            cbuf->read_ptr = 0;

        io_unblockall(&(cbuf->bqueue));
        mutex_release(&(cbuf->mutex));
    }
    return cbuf->read_ptr;
}

size_t cbuf_write(cbuf_t* cbuf, uint8_t* buffer, size_t len) {
    size_t possible, r_off;
    size_t size = cbuf->size;

    while (len > 0) {
        mutex_acquire(&(cbuf->mutex));

        r_off = cbuf->read_ptr;
        if (r_off <= cbuf->write_ptr)
            r_off += size;

        possible = cbuf->size - cbuf->write_ptr;

        if (possible > ((r_off - 1) - cbuf->write_ptr))
            possible = (r_off - 1) - cbuf->write_ptr;

        if (possible > len)
            possible = len;

        if (possible == 0) {
            mutex_release(&(cbuf->mutex));
            io_block(&(cbuf->bqueue));

            continue;
        }
        memcpy(cbuf->buffer + cbuf->write_ptr, buffer, possible);

        len -= possible;
        buffer += possible;

        cbuf->write_ptr += possible;
        if (cbuf->write_ptr == cbuf->size)
            cbuf->write_ptr = 0;

        io_unblockall(&(cbuf->bqueue));
        mutex_release(&(cbuf->mutex));
    }
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

    return wp - rp;
}