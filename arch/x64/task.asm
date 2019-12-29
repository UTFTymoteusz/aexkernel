extern task_tss
extern task_current_context
extern task_switch_stage2

global task_save_internal
task_save_internal:
    push rax
    mov rax, qword [task_current_context]

    mov qword [rax + 8 * 14], rbx

    mov rbx, rsp ; preserve rsp in rbx

    mov rsp, rax
    add rsp, 8 * 14
    
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

    fxsave [rax + 8 * 22]

    mov rsp, rbx ; restore rsp from rbx

    mov rbx, rax
    pop rax
    mov qword [rbx + 8 * 15], rax ; at last save rax

    mov rax, cr3         ; saving cr3
    mov qword [rbx], rax

    mov rax, rsp ; save rsp in rax
    add rsp, 8   ; here we skip over the return address

    pop rcx ; rip
    pop r8  ; cs
    pop r9  ; rflags
    pop r10 ; rsp
    pop r11 ; ss

    mov rsp, rbx
    add rsp, 8 * 21

    push r11 ; ss
    push r10 ; rsp
    push r9  ; rflags
    push r8  ; rsp
    push rcx ; ss

    mov rsp, rax ; restore rsp from rax
    ret

global task_enter
task_enter:
    call task_tss

    mov rsp, qword [task_current_context]
    fxrstor [rsp + 8 * 22]

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

    iretq

global task_switch_full
task_switch_full:
    int 32
    ret