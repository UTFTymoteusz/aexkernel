[BITS 64]

%macro int_err 1
    global isr%1
    isr%1:
        push byte %1
        jmp isr_common_stub
%endmacro
%macro int_noerr 1
    global isr%1
    isr%1:
        push byte 0
        push byte %1
        jmp isr_common_stub
%endmacro

int_noerr 0
int_noerr 1
int_noerr 2
int_noerr 3
int_noerr 4
int_noerr 5
int_noerr 6
int_noerr 7
int_err 8
int_noerr 9
int_err 10
int_err 11
int_err 12
int_err 13
int_err 14
int_noerr 15
int_noerr 16
int_noerr 17
int_noerr 18
int_noerr 19
int_noerr 20
int_noerr 21
int_noerr 22
int_noerr 23
int_noerr 24
int_noerr 25
int_noerr 26
int_noerr 27
int_noerr 28
int_noerr 29
int_noerr 30
int_noerr 31

extern fault_handler
isr_common_stub:
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
    call fault_handler

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

	add rsp, 16	; Cleans up pushed error code and pushed ISR number
	iretq