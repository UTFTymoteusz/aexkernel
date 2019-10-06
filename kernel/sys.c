#include "dev/cpu.h"
#include "dev/tty.h"

#include <stdio.h>

#include "sys.h"

void kpanic(char* msg) {
    tty_set_color_ansi(31);
    printf(" ! Kernel Panic !\n");
    tty_set_color_ansi(97);

    printf("%s\n", msg);
    printf("System Halted\n");

    halt();
}