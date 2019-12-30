SECTION .data

global kernel_stack
ALIGN 16
kernel_stack:
	resb 0x8000

global tss_ptr
tss_ptr:
	resb 0x72