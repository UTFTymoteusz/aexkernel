#pragma once

#define TTY_WIDTH  80
#define TTY_HEIGHT 25

#define VGA_OFFSET 0xFFFFFFFF800B0000

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

typedef struct vga_char {
    unsigned char ascii;
    unsigned char fgcolor : 4;
    unsigned char bgcolor : 4;
} __attribute__((packed)) vga_char_t;

const char ansi_to_vga[16] = {
    VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_BROWN, VGA_COLOR_BLUE, VGA_COLOR_PURPLE, VGA_COLOR_CYAN, VGA_COLOR_GRAY,
    VGA_COLOR_DARK_GRAY, VGA_COLOR_LIGHT_RED, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_YELLOW, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_PURPLE, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_WHITE,
};

vga_char_t* vga_buffer;

int tty_x, tty_y;
char vga_fg, vga_bg;

void tty_init() {
    size_t total = TTY_WIDTH * TTY_HEIGHT;

    tty_x = tty_y = 0;
    vga_bg = VGA_COLOR_BLACK;
    vga_fg = VGA_COLOR_WHITE;

    vga_buffer = (vga_char_t*)0xB8000;

    for (size_t i = 0; i < total; i++) {
        vga_buffer[i].ascii = ' ';
        vga_buffer[i].bgcolor = vga_bg;
        vga_buffer[i].fgcolor = vga_fg;
    }
}

void tty_scroll_down() {

    for (size_t y = 1; y < TTY_HEIGHT; y++)
    for (size_t x = 0; x < TTY_WIDTH; x++)
        vga_buffer[x + ((y - 1) * TTY_WIDTH)] = vga_buffer[x + (y * TTY_WIDTH)];

    for (size_t x = 0; x < TTY_WIDTH; x++)
        vga_buffer[x + ((TTY_HEIGHT - 1) * TTY_WIDTH)].ascii = ' ';
        
    if (tty_y > 0)
        tty_y--;
}

static inline void tty_verify() {
    
    if (tty_x >= TTY_WIDTH) {
        tty_x = 0;
        tty_y++;
    }

    while (tty_y >= TTY_HEIGHT)
        tty_scroll_down();
}
static inline void tty_putchar_int(char c) {

    switch (c) {
        case '\n':
            tty_x = 0;
            tty_y++;
            break;
        case '\r':
            tty_x = 0;
            break;
        default:
            vga_buffer[tty_x + (tty_y * TTY_WIDTH)].ascii = c;
            vga_buffer[tty_x + (tty_y * TTY_WIDTH)].fgcolor = vga_fg;
            vga_buffer[tty_x + (tty_y * TTY_WIDTH)].bgcolor = vga_bg;

            tty_x++;
            break;
    }
    tty_verify();
}

void tty_putchar(char c) {
    tty_putchar_int(c);
}

void tty_write(char* text) {
    size_t i = 0;
    char c = text[0];
    
    while (c) {
        tty_putchar_int(c);
        c = text[++i];
    }
}
void tty_set_color_ansi(char code) {
    if (code >= 30 && code <= 37)
        vga_fg = ansi_to_vga[code - 30];
    else if (code >= 40 && code <= 47)
        vga_bg = ansi_to_vga[code - 40];
    else if (code >= 90 && code <= 97)
        vga_fg = ansi_to_vga[code - 90 + 8];
    else if (code >= 100 && code <= 107)
        vga_bg = ansi_to_vga[code - 100 + 8];
}