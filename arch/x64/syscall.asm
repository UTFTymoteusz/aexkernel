[BITS 64]

SECTION .text
global syscall_init_asm
syscall_init_asm:

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1
    wrmsr

    mov ecx, 0xC0000081
    rdmsr
    mov edx, 0x00100008
    wrmsr

    mov ecx, 0xC0000082
    mov rdx, syscall_entry
    mov rax, rdx

    shr rdx, 32
    wrmsr 

    ret

; rdi - sys_id, rsi - a, rdx - b, r8 - c, r9 - d, r10 - e, r12 - f
extern syscall_handler
extern task_current
syscall_entry:
    push rbp
    mov rbp, rsp

    mov rax, qword [task_current]
    mov rsp, [rax]
    
    push rcx
    push r11

    push r12
    push r10
    push r9
    push r8
    push rdx
    push rsi
    push rdi

    mov rdi, rsp

    call syscall_handler

    add rsp, 8 * 7

    pop r11
    pop rcx
    
    mov rsp, rbp
    pop rbp

    o64 sysret