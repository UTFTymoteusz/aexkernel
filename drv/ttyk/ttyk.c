#include "aex/aex.h"
#include "aex/io.h"
#include "aex/mem.h"
#include "aex/spinlock.h"
#include "aex/rcode.h"
#include "aex/string.h"

#include "aex/dev/char.h"
#include "aex/dev/dev.h"
#include "aex/dev/input.h"
#include "aex/dev/tty.h"

#include "ttyk.h"

int  ttyk_open (int fd, dev_fd_t* file);
int  ttyk_read (int fd, dev_fd_t* file, uint8_t* buffer, int len);
int  ttyk_write(int fd, dev_fd_t* file, uint8_t* buffer, int len);
void ttyk_close(int fd, dev_fd_t* file);
long ttyk_ioctl(int fd, dev_fd_t* file, long code, void* mem);

struct dev_file_ops ttyk_ops = {
    .open  = ttyk_open,
    .read  = ttyk_read,
    .write = ttyk_write,
    .close = ttyk_close,
    .ioctl = ttyk_ioctl,
};

char* keymap;
bqueue_t io_queue;

spinlock_t ttyk_spinlock = {
    .val = 0,
    .name = "ttyk",
};
size_t ttyk_current_ptr = 0;

struct ttyk_queue {
    size_t size;
    dev_fd_t*  current;
    dev_fd_t** files;

    int debug_index;
};
typedef struct ttyk_queue ttyk_queue_t;

ttyk_queue_t* ttyk_queues = NULL;

static void append_file(ttyk_queue_t* queue, dev_fd_t* file) {
    queue->current = file;
    queue->size++;

    queue->files = krealloc(queue->files, queue->size * sizeof(ttyk_queue_t));
    queue->files[queue->size - 1] = file;
}

static void remove_file(ttyk_queue_t* queue, dev_fd_t* file) {
    int offset = 0;

    for (size_t i = 0; i < queue->size; i++) {
        if (queue->files[i] == file)
            offset = 1;

        if (offset == 0)
            continue;

        if (i == queue->size - 1)
            break;

        queue->files[i] = queue->files[i + offset];
    }

    if (offset == 0) {
        printk("failed to remove_file() in %s:%i", __FILE__, __LINE__);
        kpanic("failed to remove_file()");
    }
    
    queue->size--;
    queue->current = queue->files[queue->size - 1];

    queue->files = krealloc(queue->files, queue->size * sizeof(ttyk_queue_t));
}

int ttyk_open(int internal_id, dev_fd_t* file) {
    spinlock_acquire(&ttyk_spinlock);
    append_file(&(ttyk_queues[internal_id]), file);
    spinlock_release(&ttyk_spinlock);

    return 0;
}

int ttyk_read(int internal_id, dev_fd_t* file, uint8_t* buffer, int len) {
    int left = len;
    while (left > 0) {
        char c;

        uint16_t k  = 0;
        uint8_t mod = 0;

        input_kb_wait(ttyk_current_ptr);
        if (internal_id != tty_current)
            continue;

        if (ttyk_queues[internal_id].current != file)
            continue;

        spinlock_acquire(&ttyk_spinlock);
        ttyk_current_ptr = input_kb_get((uint8_t*) &k, &mod, ttyk_current_ptr);
        spinlock_release(&ttyk_spinlock);

        if (k == 0)
            continue;

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
    return len;
}

static inline void interpret_ansi(int id, char* buffer, size_t offset) {
    char UNUSED start, final;
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
            tty_set_color_ansi(id, args[0]);
            break;
        case 'J':
            if (args[0] == 2)
                tty_clear(id);

            break;
        case 'H':
            tty_set_cursor_pos(id, args[0] - 1, args[1] - 1);
            break;
    }
}

static inline void tty_write_internal(int id, char c) {
    static size_t state = 0;
    static size_t offset = 0;

    static char buffer[24];

    switch (state) {
        case 0:
            if (c == 27) {
                state = 1;
                break;
            }
            tty_putchar(id, c);
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
                interpret_ansi(id, buffer, --offset);
                offset = 0;
            }
            else if (offset > 22) {
                state  = 0;
                offset = 0;
            }
            break;
    }
}

int ttyk_write(int internal_id, UNUSED dev_fd_t* file, uint8_t* buffer, int len) {
    static spinlock_t spinlock = {
        .val = 0,
        .name = "ttyk write",
    };
    spinlock_acquire(&spinlock);

    for (int i = 0; i < len; i++)
        tty_write_internal(internal_id, buffer[i]);

    spinlock_release(&spinlock);
    return len;
}

void ttyk_close(int internal_id, dev_fd_t* file) {
    spinlock_acquire(&ttyk_spinlock);
    remove_file(&(ttyk_queues[internal_id]), file);
    spinlock_release(&ttyk_spinlock);
}

void ttyk_init() {
    io_create_bqueue(&io_queue);

    keymap = kmalloc(1024);
    input_fetch_keymap("us", keymap);

    char buffer[8];

    ttyk_queues = kzmalloc(sizeof(ttyk_queue_t) * TTY_AMOUNT);

    for (int i = 0; i < TTY_AMOUNT; i++) {
        dev_char_t* devc = kzmalloc(sizeof(dev_char_t));

        devc->ops = &ttyk_ops;
        devc->internal_id = i;

        snprintf(buffer, 8, "tty%i", i);
        dev_register_char(buffer, devc);

        ttyk_queues[i].debug_index = i;
    }
}

struct ttysize {
    uint16_t rows;
    uint16_t columns;
    uint16_t pixel_height;
    uint16_t pixel_width;
};

long ttyk_ioctl(int internal_id, UNUSED dev_fd_t* file, long code, void* mem) {
    switch (code) {
        case IOCTL_BYTES_AVAILABLE:
            spinlock_acquire(&ttyk_spinlock);
            *((int*) mem) = input_kb_available(ttyk_current_ptr);
            spinlock_release(&ttyk_spinlock);

            return 0;
        case IOCTL_TTY_SIZE:
            ;
            struct ttysize* ttysz = mem;

            int tty_width, tty_height;
            tty_get_size(internal_id, &tty_width, &tty_height);

            ttysz->columns = tty_width;
            ttysz->rows    = tty_height;
            ttysz->pixel_height = 0;
            ttysz->pixel_width  = 0;
            return 0;
        default:
            return ERR_NOT_IMPLEMENTED;
    }
}