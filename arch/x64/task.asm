extern task_tss
extern task_current_context
extern task_switch_stage2

global task_save_internal
task_save_internal:
    push rax
    mov rax, rsp

    mov rsp, qword [task_current_context]
    add rsp, 8 * 16

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
    add rsp, 8 * 21

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

    call task_tss

    mov rsp, qword [task_current_context]

    pop rax
    mov cr3, rax

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

global task_switch_full
task_switch_full:
    push rbp
    push rbx
    mov rbp, rsp

    mov rbx, ss
    push rbx

    push rbp

    pushfq
    cli

    mov rbx, cs
    push rbx

    push task_switch_full_exit

    call task_save_internal
    call task_switch_stage2

task_switch_full_exit:
    pop rbx
    pop rbp

    ret