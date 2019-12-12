#include "aex/hook.h"

#include "aex/dev/cpu.h"
#include "aex/dev/tty.h"

#include <stdio.h>

#include "aex/sys.h"

void kpanic(char* msg) {
    tty_set_color_ansi(31);
    printf(" ! Kernel Panic !\n");
    tty_set_color_ansi(97);

    printf("%s\n", msg);
    printf("System Halted\n");

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