[BITS 64]

global syscall_init_asm

extern task_cpu_locals
extern task_cpu_local_size

extern syscall_wrapper

SECTION .text
syscall_init_asm:
    mov ecx, 0xC0000080 ; Enabling syscall
    rdmsr               ;
    or eax, 1           ;
    wrmsr               ;

    mov ecx, 0xC0000081 ; Set syscall ring0 and ring3 segments
    rdmsr               ;
    mov edx, 0x00100008 ;
    wrmsr               ;

    mov ecx, 0xC0000082    ; Set syscall entry point
    mov rdx, syscall_entry ;
    mov rax, rdx           ;
    shr rdx, 32            ;
    wrmsr                  ;

    ret

; rdi - sys_id, rsi - a, rdx - b, r10 - c, r8 - d, r9 - e
syscall_entry:
    mov qword [gs:40], rsp ; Save user stack
    mov rsp, qword [gs:32] ; Load the kernel stack

    push rcx
    push r11

    mov rcx, r10 ; Move r10 to rcx so it works with the calling convention

    call syscall_wrapper

    pop r11
    pop rcx

    mov rsp, qword [gs:40] ; Load the user stack back
    o64 sysret