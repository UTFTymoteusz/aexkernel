#include "aex/dev/tty.h"
#include "aex/aex.h"
#include "aex/mem.h"
#include "aex/spinlock.h"
#include "aex/string.h"

#include <stdint.h>

#define TTY_TX_OFFSET 0xFFFFFFFF800B8000

enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_PURPLE = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_GRAY = 7,
    VGA_COLOR_DARK_GRAY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_PURPLE = 13,
    VGA_COLOR_YELLOW = 14,
    VGA_COLOR_WHITE = 15,
};

uint32_t vgag_color[16] = {
    0x000000,
    0x0000AA,
    0x00AA00,
    0x00AAAA,
    0xAA0000,
    0xAA00AA,
    0xAA5500,
    0xAAAAAA,
    0x555555,
    0x2222FF,
    0x00FF00,
    0x00FFFF,
    0xFF0000,
    0xFF00FF,
    0xFFFF00,
    0xFFFFFF,
};

const char ansi_to_vga[16] = {
    VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_BROWN, VGA_COLOR_BLUE, VGA_COLOR_PURPLE, VGA_COLOR_CYAN, VGA_COLOR_GRAY,
    VGA_COLOR_DARK_GRAY, VGA_COLOR_LIGHT_RED, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_YELLOW, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_PURPLE, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_WHITE,
};

struct tty_char {
    uint8_t ascii;
    uint8_t fgcolor : 4;
    uint8_t bgcolor : 4;
} PACKED;
typedef struct tty_char tty_char_t;

struct tty {
    volatile int cursor_x;
    volatile int cursor_y;

    int size_x;
    int size_y;

    int color_fg;
    int color_bg;

    tty_char_t* volatile buffer;
    tty_char_t* volatile output;

    spinlock_t write_lock;
};
typedef struct tty tty_t;

int tty_current = 0;
tty_t ttys[TTY_AMOUNT];

tty_char_t tty0_buffer[80 * 25 * sizeof(tty_char_t)];

spinlock_t display_lock = create_spinlock();

static void _tty_enable_cursor(uint8_t start, uint8_t end) {
    outportb(0x3D4, 0x0A);
    outportb(0x3D5, (inportb(0x3D5) & 0xC0) | start);

    outportb(0x3D4, 0x0B);
    outportb(0x3D5, (inportb(0x3D5) & 0xE0) | end);
}

static void _tty_set_cursor_pos(uint8_t x, uint8_t y) {
    uint16_t index = y * 80 + x;

    outportb(0x3D4, 0x0F);
    outportb(0x3D5, index & 0xFF);

    outportb(0x3D4, 0x0E);
    outportb(0x3D5, (index >> 8) & 0xFF);
}

void tty_init() {
    for (int i = 0; i < TTY_AMOUNT; i++) {
        ttys[i].cursor_x = 0;
        ttys[i].cursor_y = 0;
        ttys[i].size_x = 80;
        ttys[i].size_y = 25;
        ttys[i].color_fg = 15;
        ttys[i].color_bg = 0;

        ttys[i].buffer = tty0_buffer;
        ttys[i].output = (tty_char_t*) 0xFFFFFFFF800B8000;
    }
    _tty_enable_cursor(14, 15);
}

void tty_init_post() {
    for (int i = 1; i < TTY_AMOUNT; i++) {
        ttys[i].cursor_x = 0;
        ttys[i].cursor_y = 0;
        ttys[i].size_x = 80;
        ttys[i].size_y = 25;
        ttys[i].color_fg = 15;
        ttys[i].color_bg = 0;

        ttys[i].buffer = kzmalloc(ttys[i].size_x * ttys[i].size_y * sizeof(tty_char_t));
        ttys[i].output = (tty_char_t*) 0xFFFFFFFF800B8000;

        tty_clear(i);
    }
}

static void tty_scroll(int vid) {
    tty_t* tty = &(ttys[vid]);
    
    memcpy(&(tty->buffer[0 + 0 * tty->size_x]), 
           &(tty->buffer[0 + 1 * tty->size_x]), 
        tty->size_x * (tty->size_y - 1) * sizeof(tty_char_t));

    tty_char_t* tty_char;
    int start = (tty->size_y - 1) * tty->size_x;

    for (int i = 0; i < tty->size_x; i++) {
        tty_char = &(tty->buffer[start + i]);

        tty_char->ascii = '\0';
        tty_char->fgcolor = tty->color_fg;
        tty_char->bgcolor = tty->color_bg;
    }

    tty->cursor_y--;

    if (vid != tty_current)
        return;

    spinlock_acquire(&display_lock);
    memcpy(tty->output, 
           tty->buffer, 
        tty->size_x * tty->size_y * sizeof(tty_char_t));
    spinlock_release(&display_lock);
}

void tty_write(int vid, char* buffer) {
    tty_t* tty = &(ttys[vid]);
    tty_char_t* tty_char;
    int len = 0;

    spinlock_acquire(&(tty->write_lock));

    int start_x = tty->cursor_x;
    int start_y = tty->cursor_y;

    while (*buffer != '\0') {
        if (tty->cursor_x >= tty->size_x) {
            tty->cursor_x = 0;
            tty->cursor_y++;

            while (tty->cursor_y > tty->size_y - 1)
                tty_scroll(vid);
        }
        tty_char = &(tty->buffer[tty->cursor_x + tty->cursor_y * tty->size_x]);
        tty->cursor_x++;
        
        switch (*buffer) {
            case '\n':
                buffer++;
                tty->cursor_x = 0;
                tty->cursor_y++;

                while (tty->cursor_y > tty->size_y - 1)
                    tty_scroll(vid);

                continue;
            case '\r':
                buffer++;
                tty->cursor_x = 0;

                continue;
        }

        tty_char->ascii   = *buffer;
        tty_char->fgcolor = tty->color_fg;
        tty_char->bgcolor = tty->color_bg;

        len++;
        buffer++;
    }

    if (vid != tty_current) {
        spinlock_release(&(tty->write_lock));
        return;
    }
    
    spinlock_acquire(&display_lock);
    memcpy(&(tty->output[start_x + start_y * tty->size_x]), 
           &(tty->buffer[start_x + start_y * tty->size_x]), 
        len * sizeof(tty_char_t));

    _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
    spinlock_release(&display_lock);

    spinlock_release(&(tty->write_lock));
}

void tty_putchar(int vid, char c) {
    tty_t* tty = &(ttys[vid]);
    tty_char_t* tty_char;

    spinlock_acquire(&(tty->write_lock));

    if (tty->cursor_x >= tty->size_x) {
        tty->cursor_x = 0;
        tty->cursor_y++;

        while (tty->cursor_y > tty->size_y - 1)
            tty_scroll(vid);
    }

    tty_char = &(tty->buffer[tty->cursor_x + tty->cursor_y * tty->size_x]);
    if (c != '\b')
        tty->cursor_x++;
    
    switch (c) {
        case '\n':
            tty->cursor_x = 0;
            tty->cursor_y++;

            while (tty->cursor_y > tty->size_y - 1)
                tty_scroll(vid);

            if (vid == tty_current) 
                _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
            
            spinlock_release(&(tty->write_lock));
            return;
        case '\r':
            tty->cursor_x = 0;

            if (vid == tty_current) 
                _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
            
            spinlock_release(&(tty->write_lock));
            return;
        case '\b':
            if (tty->cursor_x == 0) {
                _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
                spinlock_release(&(tty->write_lock));
                return;
            }
            tty_char->ascii = ' ';
            tty->cursor_x--;

            if (vid == tty_current) 
                _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
            
            spinlock_release(&(tty->write_lock));
            return;
    }
    int start_x = tty->cursor_x - 1;
    int start_y = tty->cursor_y;

    tty_char->ascii   = c;
    tty_char->fgcolor = tty->color_fg;
    tty_char->bgcolor = tty->color_bg;

    if (vid != tty_current) {
        spinlock_release(&(tty->write_lock));
        return;
    }
    spinlock_acquire(&display_lock);
    tty->output[start_x + start_y * tty->size_x] = tty->buffer[start_x + start_y * tty->size_x];
    _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
    spinlock_release(&display_lock);

    spinlock_release(&(tty->write_lock));
}

void tty_clear(int vid) {
    tty_t* tty = &(ttys[vid]);
    spinlock_acquire(&(tty->write_lock));

    tty_char_t* tty_char;

    for (int i = 0; i < tty->size_x * tty->size_y; i++) {
        tty_char = &(tty->buffer[i]);

        tty_char->ascii   = '\0';
        tty_char->fgcolor = tty->color_fg;
        tty_char->bgcolor = tty->color_bg;
    }

    if (vid != tty_current) {
        spinlock_release(&(tty->write_lock));
        return;
    }

    spinlock_acquire(&display_lock);
    memcpy(tty->output, 
           tty->buffer, 
        tty->size_x * tty->size_y * sizeof(tty_char_t));
    spinlock_release(&display_lock);

    spinlock_release(&(tty->write_lock));
}

void tty_set_color_ansi(int vid, char ansi) {
    tty_t* tty = &(ttys[vid]);
    spinlock_acquire(&(tty->write_lock));

    if (ansi >= 30 && ansi <= 37)
        tty->color_fg = ansi_to_vga[ansi - 30];
    else if (ansi >= 40 && ansi <= 47)
        tty->color_bg = ansi_to_vga[ansi - 40];
    else if (ansi >= 90 && ansi <= 97)
        tty->color_fg = ansi_to_vga[ansi - 90 + 8];
    else if (ansi >= 100 && ansi <= 107)
        tty->color_bg = ansi_to_vga[ansi - 100 + 8];

    spinlock_release(&(tty->write_lock));
}

void tty_get_size(int vid, int* size_x, int* size_y) {
    tty_t* tty = &(ttys[vid]);

    *size_x = tty->size_x;
    *size_y = tty->size_y;
}

void tty_set_cursor_pos(int vid, int x, int y) {
    tty_t* tty = &(ttys[vid]);
    spinlock_acquire(&(tty->write_lock));

    if (tty->cursor_x >= 0 || tty->cursor_x < tty->size_x)
        tty->cursor_x = x;
        
    if (tty->cursor_y >= 0 || tty->cursor_y < tty->size_y)
        tty->cursor_y = y;

    if (vid == tty_current)
        _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);

    spinlock_release(&(tty->write_lock));
}

void tty_switch_to(int vid) {
    tty_current = vid;
    tty_t* tty = &(ttys[vid]);

    spinlock_acquire(&display_lock);
    memcpy(tty->output, 
           tty->buffer, 
        tty->size_x * tty->size_y * sizeof(tty_char_t));
        
    _tty_set_cursor_pos(tty->cursor_x, tty->cursor_y);
    spinlock_release(&display_lock);
}