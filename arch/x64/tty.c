#include "boot/multiboot.h"

#include "dev/cpu.h"

#include "mem/page.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "dev/tty.h"

#define VGA_TX_OFFSET 0xFFFFFFFF800B8000
#define VGA_GR_OFFSET 0xFFFFFFFF800A0000

extern void* _binary_boot_font_psf_start;

size_t tty_width, tty_height;

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
    0x5555FF,
    0x55FF55,
    0x55FFFF,
    0xFF5555,
    0xFF55FF,
    0xFFFF55,
    0xFFFFFF,
};

typedef struct vga_char {
    uint8_t ascii;
    uint8_t fgcolor : 4;
    uint8_t bgcolor : 4;
} __attribute__((packed)) vga_char_t;

const char ansi_to_vga[16] = {
    VGA_COLOR_BLACK, VGA_COLOR_RED, VGA_COLOR_GREEN, VGA_COLOR_BROWN, VGA_COLOR_BLUE, VGA_COLOR_PURPLE, VGA_COLOR_CYAN, VGA_COLOR_GRAY,
    VGA_COLOR_DARK_GRAY, VGA_COLOR_LIGHT_RED, VGA_COLOR_LIGHT_GREEN, VGA_COLOR_YELLOW, VGA_COLOR_LIGHT_BLUE, VGA_COLOR_LIGHT_PURPLE, VGA_COLOR_LIGHT_CYAN, VGA_COLOR_WHITE,
};

uint8_t* font;

volatile vga_char_t* vga_tx_buffer = (vga_char_t*)VGA_TX_OFFSET;
void* vga_gr_buffer;

#define put_pixel(x, y, c) *((uint32_t*)(vga_gr_buffer + y * px_ysize + x * px_xsize)) = c;

size_t tty_x, tty_y;
size_t vga_fg, vga_bg;
uint32_t vgag_bg, vgag_fg;

size_t px_xsize, px_ysize;
size_t gr_width, gr_height;

size_t widthl, heightl;

int mode = 0;
bool gp_init = false;

volatile vga_char_t* tmp_buffer[2048];

void* memcpy_cpusz(void* restrict dstptr, const void* restrict srcptr, size_t size) {
	size_t* dst = (size_t*) dstptr;
	const size_t* src = (const size_t*) srcptr;

    size = (size + sizeof(size_t) - 1) / sizeof(size_t);
	for (size_t i = 0; i < size; i++)
		dst[i] = src[i];

	return dstptr;
}

void* memset_cpusz(void* bufptr, int value, size_t size) {
	size_t* buf = (size_t*) bufptr;

    size = (size + sizeof(size_t) - 1) / sizeof(size_t);
	for (size_t i = 0; i < size; i++)
		buf[i] = (size_t) value;

	return bufptr;
}

void tty_init_multiboot(multiboot_info_t* mbt) {
    tty_x  = tty_y = 0;
    vga_bg = VGA_COLOR_BLACK;
    vga_fg = VGA_COLOR_WHITE;

    tty_width = 80;
    tty_height = 25;

    if ((mbt->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) && mbt->framebuffer_type != 2) {
        if (mbt->framebuffer_bpp != 32)
            outportb(0x64, 0xFE); // temp

        vga_gr_buffer = (void*)mbt->framebuffer_addr;

        px_xsize = 4;
        px_ysize = mbt->framebuffer_pitch;

        widthl  = mbt->framebuffer_width / 8;
        heightl = mbt->framebuffer_height / 16;

        gr_width  = mbt->framebuffer_width;
        gr_height = mbt->framebuffer_height;

        vga_tx_buffer = tmp_buffer;

        gp_init = true;
    }
    mode = 1;
    size_t total = tty_width * tty_height;
    for (size_t i = 0; i < total; i++) {
        vga_tx_buffer[i].ascii = ' ';
        vga_tx_buffer[i].bgcolor = vga_bg;
        vga_tx_buffer[i].fgcolor = vga_fg;
    }
}

struct psf_header {
    uint16_t magic;
    uint8_t mode;
    uint8_t charsize;
} __attribute((packed));
typedef struct psf_header psf_header_t;

void tty_load_psf() {
    size_t start = (size_t)&_binary_boot_font_psf_start;
    psf_header_t* header = (psf_header_t*)start;

    font = (uint8_t*)(start + 4);
}

inline static void draw_char(int x, int y, char c) {
    x *= 8;
    y *= 16;

    size_t off = c;

    uint8_t tr;
    size_t addr = (size_t)vga_gr_buffer + y * px_ysize + x * px_xsize;

    for (uint8_t yi = 0; yi < 16; yi++) {
        tr = font[(off * 16) + yi];
        for (uint8_t xi = 0; xi < 8; xi++) {
            if (tr & (0b10000000 >> xi))
                *((uint32_t*)addr) = vgag_fg;
            else
                *((uint32_t*)addr) = vgag_bg;

            addr += px_xsize;
        }
        addr += gr_width * px_xsize;
        addr -= px_xsize * 8;
    }
}

void tty_init_post() {
    if (!gp_init)
        return;

    tty_load_psf();

    size_t size = (gr_height) * (gr_width * px_xsize); // Figure out why adding pitch here messes it up
    vga_gr_buffer = mempg_mapto(mempg_to_pages(size), vga_gr_buffer, NULL, 0x03);

    mode = 2;

    for (size_t y = 0; y < gr_height; y++)
    for (size_t x = 0; x < gr_width; x++)
        put_pixel(x, y, 0x000000);

    for (size_t y = 0; y < tty_height; y++)
    for (size_t x = 0; x < tty_width; x++) {
        vgag_bg = vgag_color[vga_tx_buffer[x + (y * tty_width)].bgcolor];
        vgag_fg = vgag_color[vga_tx_buffer[x + (y * tty_width)].fgcolor];
        draw_char(x, y, vga_tx_buffer[x + (y * tty_width)].ascii);
    }
    tty_width  = widthl;
    tty_height = heightl;

    vgag_bg = vgag_color[vga_bg];
    vgag_fg = vgag_color[vga_fg];
}

void tty_scroll_down() {
    switch (mode) {
        case 2:
            ;
            size_t off = 16 * gr_width * px_xsize;

            for (size_t y = 1; y < tty_height; y++)
                memcpy_cpusz(vga_gr_buffer + (y - 1) * off, vga_gr_buffer + y * off, gr_width * px_xsize * 16);

            memset_cpusz(vga_gr_buffer + ((tty_height - 1) * off), 0x000000, gr_width * px_xsize * 16);

            if (tty_y > 0)
                tty_y--;
            break;
        default:
            for (size_t y = 1; y < tty_height; y++)
            for (size_t x = 0; x < tty_width; x++)
                vga_tx_buffer[x + ((y - 1) * tty_width)] = vga_tx_buffer[x + (y * tty_width)];

            for (size_t x = 0; x < tty_width; x++)
                vga_tx_buffer[x + ((tty_height - 1) * tty_width)].ascii = ' ';
                
            if (tty_y > 0)
                tty_y--;
            break;
    }
}

static inline void tty_verify() {
    if (tty_x >= tty_width) {
        tty_x = 0;
        tty_y++;
    }
    while (tty_y >= tty_height)
        tty_scroll_down();
}

static inline void tty_putchar_int_tx(char c) {
    switch (c) {
        case '\n':
            tty_x = 0;
            tty_y++;
            break;
        case '\r':
            tty_x = 0;
            break;
        case '\b':
            if (tty_x == 0)
                break;

            --tty_x;
            break;
        default:
            vga_tx_buffer[tty_x + (tty_y * tty_width)].ascii = c;
            vga_tx_buffer[tty_x + (tty_y * tty_width)].fgcolor = vga_fg;
            vga_tx_buffer[tty_x + (tty_y * tty_width)].bgcolor = vga_bg;

            tty_x++;
            break;
    }
    tty_verify();
}

static inline void tty_putchar_int_gr(char c) {
    switch (c) {
        case '\n':
            tty_x = 0;
            tty_y++;
            break;
        case '\r':
            tty_x = 0;
            break;
        case '\b':
            if (tty_x == 0)
                break;

            --tty_x;
            break;
        default:
            draw_char(tty_x, tty_y, c);
            tty_x++;
            break;
    }
    tty_verify();
}

void tty_putchar(char c) {
    switch (mode) {
        case 1:
            tty_putchar_int_tx(c);
            break;
        case 2:
            tty_putchar_int_gr(c);
            break;
        default:
            break;
    }
}

void tty_write(char* text) {
    size_t i = 0;
    char c = text[0];
    
    switch (mode) {
        case 1:
            while (c) {
                tty_putchar_int_tx(c);
                c = text[++i];
            }
            break;
        case 2:
            while (c) {
                tty_putchar_int_gr(c);
                c = text[++i];
            }
            break;
        default:
            break;
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

    vgag_bg = vgag_color[vga_bg];
    vgag_fg = vgag_color[vga_fg];
}