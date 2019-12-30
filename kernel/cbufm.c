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

    io_create_bqueue(&(cbufm->bqueue));

    return 0;
}

size_t cbufm_read(cbufm_t* cbufm, uint8_t* buffer, size_t start, size_t len) {
    size_t possible, wdoff;
    size_t size = cbufm->size;
    //printk("read: %i\n", len);

    while (len > 0) {
        mutex_wait(&(cbufm->mutex));

        wdoff = cbufm->write_ptr;
        if (wdoff < start)
            wdoff += size;

        possible = cbufm->size - start;

        if (possible > (wdoff - start))
            possible = wdoff - start;

        if (possible > len)
            possible = len;

        //printk("rposs: %i, start at: %i, len: %i\n", possible, start, len);
        //printk("a: %i, b: %i\n", wdoff, cbufm->write_ptr);
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
    //printk("rptr: %i\n", cbuf->read_ptr);
    //printk("end %i\n", start);
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

        //printk("writing %i\n", amnt);

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
    else
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