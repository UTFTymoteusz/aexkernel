#include "aex/debug.h"
#include "aex/kernel.h"
#include "aex/string.h"

#include "aex/dev/tty.h"
#include "aex/sys/cpu.h"

#include "aex/proc/task.h"

char* exception_messages[] = {
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

    "x87 Floating-Point Fault",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Fault",
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
    ((uint8_t*) 0xFFFFFFFF800B8000)[0] = 'F';
    ((uint8_t*) 0xFFFFFFFF800B8000)[1] = 'F';

    if (r->int_no == 19) {
        printk("%${93}SSE Fault%${97}\n");

        uint32_t mxcsr = 0xFFFF;
        asm volatile("stmxcsr %[mxcsr]" : [mxcsr] "=&m"(mxcsr));
        printk("MXCSR: 0x%x\n", mxcsr);
    }
        
    if (r->int_no < 32) {
        printk("%${93}%s Exception%${97}, Code: %${91}0x%04lX%${97}\n", exception_messages[r->int_no], r->err);

        if (r->int_no == 14) {
            uint64_t boi;
            
            asm volatile("mov rax, cr2;" : "=a"(boi));
            printk("CR2: 0x%016lX\n", boi);

            asm volatile("mov rax, cr3;" : "=a"(boi));
            printk("CR3: 0x%016lX\n", boi);
        }
        printk("RIP: 0x%016lX <%s>\n", (size_t) r->rip, debug_resolve_symbol((void*) r->rip));
        debug_stacktrace();

        printk("PID: %i, TID: %i\n", CURRENT_PID, CURRENT_TID);

        debug_print_registers();
        printk("System halted\n");
        halt();
    }
}