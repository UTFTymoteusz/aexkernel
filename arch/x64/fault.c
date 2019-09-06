#pragma once

#include <stdio.h>
#include <string.h>

#include "cpu.c"
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

char isrbuffer[16];
void fault_handler(struct regs* r) {
    printf("INT %s\n", itoa(r->int_no, isrbuffer, 10));
        
    if (r->int_no < 32) {
        tty_set_color_ansi(93);
        printf("%s Exception\n", exception_messages[r->int_no]);
        tty_set_color_ansi(97);

        printf("System halted\n");
        halt();
    }
}