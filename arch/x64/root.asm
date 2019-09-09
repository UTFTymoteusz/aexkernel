[BITS 64]
SECTION .text

%include "arch/x64/idt.asm"
%include "arch/x64/isr.asm"
%include "arch/x64/irq.asm"
%include "arch/x64/syscall.asm"