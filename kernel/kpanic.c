#include "aex/debug.h"
#include "aex/dev/tty.h"
#include "aex/kernel.h"
#include "aex/sys/cpu.h"

__attribute((noreturn)) void kpanic(char* msg) {
    ((uint8_t*) 0xFFFFFFFF800B8000)[0] = 'P';
    ((uint8_t*) 0xFFFFFFFF800B8000)[1] = 'P';

    nointerrupts();
    
    tty_set_color_ansi(0, 31);
    printk(" ! Kernel Panic !\n");
    tty_set_color_ansi(0, 97);

    printk("%s\n", msg);
    printk("System Halted\n");

    debug_print_registers();

    debug_stacktrace();

    halt();
}