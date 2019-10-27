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

; r12 - sys_id, rdi - a, rsi - b, rdx - c, r8 - d, r9 - e, r10 - f
extern syscalls
extern task_current
syscall_entry:
    push rcx
    push r11
    push rbp

    mov rbp, rsp

    mov rax, qword [task_current] ; Reads the kernel stack pointer
    mov rsp, [rax]

    push rdx

    mov rax, 8
    o64 mul r12

    pop rdx

    add rax, syscalls
    mov rax, [rax]

    cmp rax, 0
    je nothing_here

    cmp rax, 128
    jge nothing_here

    push r8 ; This here swaps the registers so that it works with the sysv calling convention
    push r9
    ;push r10
    ;pop r9
    pop r8
    pop rcx

    call rax

nothing_here:
    mov rsp, rbp

    pop rbp
    pop r11
    pop rcx

    o64 sysret