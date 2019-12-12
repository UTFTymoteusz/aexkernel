#include <stdio.h>

#include "aex/dev/cpu.h"
#include "aex/dev/tty.h"

#include <string.h>

#include "kernel/init.h"

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