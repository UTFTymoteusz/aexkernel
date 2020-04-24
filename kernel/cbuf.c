#include "aex/event.h"
#include "aex/kernel.h"
#include "aex/mem.h"
#include "aex/string.h"

#include "aex/proc/task.h"

#include "aex/cbuf.h"

int cbuf_create(cbuf_t* cbuf, size_t size) {
    memset(cbuf, 0, sizeof(cbuf_t));

    cbuf->size   = size;
    cbuf->buffer = kmalloc(size);
    cbuf->spinlock.val  = 0;
    cbuf->spinlock.name = NULL;

    return 0;
}

size_t cbuf_read(cbuf_t* cbuf, uint8_t* buffer, size_t len) {
    size_t possible, w_off;
    size_t size = cbuf->size;

    while (len > 0) {
        spinlock_acquire(&(cbuf->spinlock));

        w_off = cbuf->write_ptr;
        if (w_off < cbuf->read_ptr)
            w_off += size;

        possible = cbuf->size - cbuf->read_ptr;

        if (possible > (w_off - cbuf->read_ptr))
            possible = w_off - cbuf->read_ptr;

        if (possible > len)
            possible = len;

        if (possible == 0) {
            event_wait(&cbuf->event);
            spinlock_release(&(cbuf->spinlock));

            task_tyield();
            continue;
        }
        memcpy(buffer, cbuf->buffer + cbuf->read_ptr, possible);

        len -= possible;
        buffer += possible;

        cbuf->read_ptr += possible;
        if (cbuf->read_ptr == cbuf->size)
            cbuf->read_ptr = 0;

        event_trigger_all(&cbuf->event);
        spinlock_release(&(cbuf->spinlock));
    }
    return cbuf->read_ptr;
}

size_t cbuf_write(cbuf_t* cbuf, uint8_t* buffer, size_t len) {
    size_t possible, r_off;
    size_t size = cbuf->size;

    while (len > 0) {
        spinlock_acquire(&(cbuf->spinlock));

        r_off = cbuf->read_ptr;
        if (r_off <= cbuf->write_ptr)
            r_off += size;

        possible = cbuf->size - cbuf->write_ptr;

        if (possible > ((r_off - 1) - cbuf->write_ptr))
            possible = (r_off - 1) - cbuf->write_ptr;

        if (possible > len)
            possible = len;

        if (possible == 0) {
            event_wait(&cbuf->event);
            spinlock_release(&(cbuf->spinlock));

            task_tyield();
            continue;
        }
        memcpy(cbuf->buffer + cbuf->write_ptr, buffer, possible);

        len -= possible;
        buffer += possible;

        cbuf->write_ptr += possible;
        if (cbuf->write_ptr == cbuf->size)
            cbuf->write_ptr = 0;

        event_trigger_all(&cbuf->event);
        spinlock_release(&(cbuf->spinlock));
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