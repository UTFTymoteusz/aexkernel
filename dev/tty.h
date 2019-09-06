#pragma once

#include "tty.c"

// Initializes and clears the root terminal
void tty_init();

// Writes characters to the root terminal
void tty_write(char* text);

// Writes a character to the root terminal
void tty_putchar(char c);

// Enables/disables the root terminal cursor
void tty_cursor(bool enabled);

// Scrolls down the root terminal once
void tty_scroll_down();

// Sets the root terminal foreground/background color according to ANSI color codes
void tty_set_color_ansi(char code);