global entry_caller
entry_caller:
    call rax
    
    mov rdi, 2
    mov rsi, rax
    syscall