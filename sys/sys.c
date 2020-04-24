#include "aex/debug.h"
#include "aex/hook.h"
#include "aex/kernel.h"
#include "aex/syscall.h"

#include "aex/dev/input.h"
#include "aex/dev/tty.h"

#include "aex/sys/cpu.h"

#include <stdbool.h>

#include "aex/vals/sysvar_names.h"
#include "aex/sys/sys.h"

void shutdown() {
    static bool shutting_down = false;
    if (shutdown_func == NULL || shutting_down)
        return;

    shutting_down = true;

    hook_invoke(HOOK_SHUTDOWN, NULL);
    shutdown_func();

    kpanic("shutdown failed\n");
}

void register_shutdown(void* func) {
    shutdown_func = func;
}

int64_t syscall_sysvar(int id) {
    switch (id) {
        case SYSVAR_PAGESIZE:
            return CPU_PAGE_SIZE;
        case SYSVAR_TTYAMOUNT:
            return TTY_AMOUNT;
    }
    return 0xFFFFFFFFFFFFFFFF;
}

void sys_funckey(uint8_t key) {
    int tty = key - 0x6A;
    if (tty >= TTY_AMOUNT)
        return;

    tty_switch_to(tty);
}

void sys_sysrq(uint8_t key) {
    switch (key) {
        case 'S':
            printk(PRINTK_WARN "sysrq: Shutdown\n");
            shutdown();
            break;
    }
}

void sys_init() {
    syscalls[SYSCALL_SYSVAR] = syscall_sysvar;
}