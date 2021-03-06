[BITS 64]

extern task_reshed_irq

%macro irq 1
    global irq%1
    irq%1:
        push byte (%1 + 32)
        jmp irq_common_stub
%endmacro
%macro irq_stupid 1
    global irq%1
    irq%1:
        iretq
%endmacro

global irq0
irq0:
    jmp task_reshed_irq

irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq_stupid 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15

extern irq_handler
irq_common_stub:
    cli
    
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    mov rdi, rsp

    xor rbp, rbp
    call irq_handler

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
    
    add rsp, 8
    iretq