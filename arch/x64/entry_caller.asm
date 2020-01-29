global entry_caller
entry_caller:
    call rax
    
    mov r12, 2
    mov rdi, rax
    syscall