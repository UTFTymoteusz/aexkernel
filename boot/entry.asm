[BITS 64]
SECTION .text
global kernel_entry
kernel_entry:
	extern main
	call main

	halt:
		hlt
		jmp halt