#include "aex/debug.h"
#include "aex/hook.h"
#include "aex/kernel.h"

#include "aex/dev/cpu.h"
#include "aex/dev/tty.h"

#include "aex/sys.h"

void kpanic(char* msg) {
    tty_set_color_ansi(31);
    printk(" ! Kernel Panic !\n");
    tty_set_color_ansi(97);

    printk("%s\n", msg);
    printk("System Halted\n");

    debug_print_registers();

    nointerrupts();

    debug_stacktrace();

    halt();
}

void shutdown() {
    static bool shutting_down = false;
    if (shutdown_func == NULL || shutting_down)
        return;

    shutting_down = true;

    hook_invoke(HOOK_SHUTDOWN, NULL);
    shutdown_func();
}

void register_shutdown(void* func) {
    shutdown_func = func;
}