#pragma once

#include <stdio.h>

#define OS_NAME "AEX"
#define OS_VERSION "0.26"

#define DEFAULT_COLOR 97
#define HIGHLIGHT_COLOR 93

void init_print_osinfo() {
	
	char stringbuffer[32];

	printf("    Starting %s/%s\n", OS_NAME, OS_VERSION);
	printf("Running on "); 
	tty_set_color_ansi(HIGHLIGHT_COLOR);
    printf(CPU_ARCH);
	tty_set_color_ansi(DEFAULT_COLOR);
    printf(" architecture, vendor ");
	tty_set_color_ansi(HIGHLIGHT_COLOR);
	printf(cpu_get_vendor(stringbuffer));
	tty_set_color_ansi(DEFAULT_COLOR);
	printf(" processor\n\n");
}