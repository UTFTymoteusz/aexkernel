#include <stdio.h>
#include <string.h>

#include "dev/cpu.h"
#include "dev/tty.h"

char* exception_messages[] =
{
    "Division By Zero",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",

    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Unknown Interrupt",

    "Coprocessor Fault",
    "Alignment Check",
    "Machine Check",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",

    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
};

void fault_handler(struct regs* r) {
    printf("INT %i\n", r->int_no);
        
    if (r->int_no < 32) {
        tty_set_color_ansi(93);
        printf("%s Exception", exception_messages[r->int_no]);
        tty_set_color_ansi(97);

        printf(", Code: ");
        
        tty_set_color_ansi(91);
        printf("0x%x", r->err);
        tty_set_color_ansi(97);
        printf("\n");

        printf("System halted\n");
        halt();
    }
}