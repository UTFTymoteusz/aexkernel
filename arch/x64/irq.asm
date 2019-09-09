[BITS 64]

%macro irq 1
    global irq%1
    irq%1:
        push byte 0
        push byte (%1 + 32)
        jmp irq_common_stub
%endmacro

global irq0
irq0:
    jmp timerbong

irq 1
irq 2
irq 3
irq 4
irq 5
irq 6
irq 7
irq 8
irq 9
irq 10
irq 11
irq 12
irq 13
irq 14
irq 15

extern irq_handler
extern task_tss
extern task_current_context
extern task_switch
irq_common_stub:
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
    
    add rsp, 16
    iretq

timerbong:

    ;xchg bx, bx
    call task_save_internal

    mov al, 0x20
    out 0x20, al

    call task_tss
    call task_switch

    iretq

global task_save
task_save:
    mov rbx, ss
    push rbx
    push rsp
    pushfq
    mov rbx, cs
    push rbx
    push rax


task_save_internal:
    push rax
    mov rax, rsp

    mov rsp, qword [task_current_context]
    add rsp, (8 * 15)

    push rax ; make rax actually work pls
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

    mov rsp, rax
    pop rax
    
    ; rax, rip, cs, rflags, rsp, ss
    push rax
    push r15
    mov r15, rsp

    add rsp, 8
    pop r8 ; rax

    add rsp, 8
    pop r9  ; rip
    pop r10 ; cs
    pop r11 ; rflags
    pop r12 ; rsp
    pop r13 ; ss

    mov rsp, qword [task_current_context]
    add rsp, (8 * 20)

    push r13
    push r12
    push r11
    push r10
    push r9
    push r8

    mov rsp, r15
    pop r15
    pop rax

    ret

global task_enter
task_enter:
    ;xchg bx, bx
    ;add rsp, 8 ; clean up the useless stack frame and return pointer we wont ever need 

    mov rsp, qword [task_current_context]
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

    ;xchg bx, bx
    iretq