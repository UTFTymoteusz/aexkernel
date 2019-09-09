#pragma once

#include "kernel/debug.h"

#define SYSCALL_AMOUNT 32

struct syscall_args {
    size_t sys_id;
    size_t a, b, c, d, e, f;
};

void* syscall_handlers[SYSCALL_AMOUNT];

void syscall_init() {

    for (size_t i = 0; i < SYSCALL_AMOUNT; i++)
        syscall_handlers[i] = NULL;
}

void syscall_handler(struct syscall_args* args) {

    write_debug("Syscall %s\n", args->sys_id, 10);
    write_debug(" A: 0x%s\n", args->a, 16);
    write_debug(" B: 0x%s\n", args->b, 16);
    write_debug(" C: 0x%s\n", args->c, 16);
    write_debug(" D: 0x%s\n", args->d, 16);
    write_debug(" E: 0x%s\n", args->e, 16);
    write_debug(" F: 0x%s\n\n", args->f, 16);

    printf("SYSCALLS BOII\n");
}