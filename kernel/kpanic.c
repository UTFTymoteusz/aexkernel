#include "aex/debug.h"
#include "aex/dev/tty.h"
#include "aex/kernel.h"
#include "aex/sys/cpu.h"

__attribute((noreturn)) void kpanic(char* msg) {
    tty_set_color_ansi(0, 31);
    printk(" ! Kernel Panic !\n");
    tty_set_color_ansi(0, 97);

    printk("%s\n", msg);
    printk("System Halted\n");

    debug_print_registers();

    nointerrupts();
    debug_stacktrace();

    halt();
}