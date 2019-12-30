#include "aex/kernel.h"

#include "aex/dev/cpu.h"
#include "aex/dev/tty.h"

#include "kernel/init.h"

void init_print_osinfo() {
    char stringbuffer[32];

	printk("    Starting %s/%s\n", OS_NAME, OS_VERSION);
	printk("Running on "); 
	tty_set_color_ansi(HIGHLIGHT_COLOR);
    printk(CPU_ARCH);
	tty_set_color_ansi(DEFAULT_COLOR);
    printk(" architecture, vendor ");
	tty_set_color_ansi(HIGHLIGHT_COLOR);
	printk(cpu_get_vendor(stringbuffer));
	tty_set_color_ansi(DEFAULT_COLOR);
	printk(" processor\n\n");
}