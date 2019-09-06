[BITS 64]
SECTION .text
global kernel_entry
kernel_entry:
	extern main
	call main

	halt:
		hlt
		jmp halt

SECTION .bss
	align 16
global kernel_stack
kernel_stack:
	resb 8192