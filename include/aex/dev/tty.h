#pragma once

#define TTY_AMOUNT 8

// Current terminal variable.
int tty_current;

/*
 * Initializes terminal 0. Should be only used in kernel_main().
 */
void tty_init();

/*
 * Initializes other terminals and enables them if in graphical mode. 
 * Should be only used in kernel_main().
 */
void tty_init_post();

/*
 * Writes a null terminated string to the specified terminal.
 */
void tty_write();

/*
 * Writes a character out to the specified terminal.
 */
void tty_putchar(int vid, char c);

/*
 * Clears out a terminal.
 */
void tty_clear(int vid);

/*
 * Sets the color of a terminal according to ANSI escape codes.
 */
void tty_set_color_ansi(int vid, char ansi);

/*
 * Gets the size of a terminal in characters.
 */
void tty_get_size(int vid, int* size_x, int* size_y);

/*
 * Sets the cursor position of a terminal.
 */
void tty_set_cursor_pos(int vid, int x, int y);

/*
 * Switches to a different terminal.
 */
void tty_switch_to(int vid);