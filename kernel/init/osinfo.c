#include "aex/kernel.h"

#include "aex/dev/tty.h"
#include "aex/sys/cpu.h"

#include "kernel/init.h"

#define _STRING(x) #x
#define STRING(x) _STRING(x)

void init_print_osinfo() {
    char stringbuffer[32];

	printk("    Starting %s/%s\n", OS_NAME, OS_VERSION);
	printk("Running on %${"STRING(HIGHLIGHT_COLOR)"}%s%${"STRING(DEFAULT_COLOR)"} architecture, "
		   "vendor %${"STRING(HIGHLIGHT_COLOR)"}%s%${"STRING(DEFAULT_COLOR)"} processor\n\n", 
		   CPU_ARCH, cpu_get_vendor(stringbuffer)); 
}