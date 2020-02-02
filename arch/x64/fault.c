#include "aex/debug.h"
#include "aex/kernel.h"
#include "aex/string.h"

#include "aex/dev/tty.h"
#include "aex/sys/cpu.h"

#include "aex/proc/proc.h"
#include "aex/proc/task.h"

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
    printk("INT %li\n", r->int_no);
        
    if (r->int_no < 32) {
        printk("%${93}%s Exception%${97}, Code: %${91}0x%04lX%${97}\n", exception_messages[r->int_no], r->err);

        if (r->int_no == 14) {
            uint64_t boi;
            
            asm volatile("mov rax, cr2;" : "=a"(boi));
            printk("CR2: 0x%016lX\n", boi);

            asm volatile("mov rax, cr3;" : "=a"(boi));
            printk("CR3: 0x%016lX\n", boi);
        }
        printk("RIP: 0x%016lX\n", (size_t) r->rip);
        debug_stacktrace();

        if (task_current != NULL && process_current != NULL) {
            printk("Process: %li [%s], Thread: %li\n", task_current->process->pid, task_current->process->name, task_current->thread->id);
            halt();
        }
        debug_print_registers();
        printk("System halted\n");
        halt();
    }
}