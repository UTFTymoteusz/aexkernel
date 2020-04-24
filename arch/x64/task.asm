global task_shed_save
global task_shed_enter

global task_reshed_irq
global task_reshed

extern task_shed_choose
extern time_tick

extern task_current_context

extern task_cpu_locals
extern task_cpu_local_size

def_mxcsr:
    dd 0b0001111110000000

task_shed_save:
    push rbx

    mov rbx, qword [gs:16] ; Get pointer of the current context

    mov qword [rbx + 8 * 15], rax
    mov qword [rbx + 8 * 13], rcx
    mov qword [rbx + 8 * 12], rdx

    pop rax                       ; poping pushed rbx to rax
    mov qword [rbx + 8 * 14], rax ; Saving rbx
    
    mov rax, cr3         ; Saving cr3
    mov qword [rbx], rax

    mov rdx, rsp ; Saving rsp

    mov rsp, rbx    ; Set rsp to context
    add rsp, 8 * 12 ; Target the stuff we actually care about
    
    push rsi ; Save the other registers
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

    fxsave [rbx + 8 * 22]

    mov rsp, rdx ; Restore rsp
    add rsp, 8   ; Skipping over the return address

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

    mov rsp, rdx
    ret

task_shed_enter:
    mov rsp, qword [gs:16] ; Get pointer of the current context

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

task_reshed_irq:
    call task_shed_save

    ldmxcsr [def_mxcsr]

    call time_tick
    call task_shed_choose

    mov al, 0x20
    out 0x20, al

    jmp task_shed_enter

    iretq

task_reshed:
    push rbp
    mov rbp, rsp

    mov rdi, ss
    push rdi

    push rbp

    pushfq

    mov rdi, cs
    push rdi

    push .exit

    cli

    call task_shed_save
    
    ldmxcsr [def_mxcsr]
    call task_shed_choose

    jmp task_shed_enter

    .exit:
        pop rbp
        ret