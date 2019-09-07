[BITS 32]
SECTION .text

STARTING_PAGE_AMOUNT equ 0xF000

global _start
_start:
	jmp bootstrap

ALIGN 4
mboot:
	MULTIBOOT_PAGE_ALIGN	equ 1<<0
	MULTIBOOT_MEMORY_INFO	equ 1<<1
	MULTIBOOT_AOUT_KLUDGE	equ 1<<16
	MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
	MULTIBOOT_HEADER_FLAGS  equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_AOUT_KLUDGE
	MULTIBOOT_CHECKSUM	equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
	EXTERN	code, bss, end

	dd MULTIBOOT_HEADER_MAGIC
	dd MULTIBOOT_HEADER_FLAGS
	dd MULTIBOOT_CHECKSUM

	dd mboot ; header
	dd code  ; load addr
	dd bss   ; load addr end
	dd end   ; bss addr end
	dd _start ; entrypoint

extern kernel_entry

you_suck:
	dd " CPU is not AMD64/x64, buy a newer one", 0
	
bootstrap:
	push eax
	push ebx
	push ecx
	push edx
	push esi
	push edi

	mov eax, 0x80000001
	cpuid

	and edx, 1 << 29
	cmp edx, 1
	jge longmode_is_a_thing

	mov eax, 0
	mov esi, you_suck
	mov edi, 0xB8000

again_msg:
	mov al, byte [esi]
	mov byte [edi], al

	inc edi
	mov byte [edi], 0x1F

	inc esi
	inc edi

	cmp al, 0
	jne again_msg

again_fill:
	mov byte [edi], ' '
	inc edi
	mov byte [edi], 0x1F
	inc edi

	cmp edi, (0xB8000 + 80 * 50)
	jl again_fill

	hlt

longmode_is_a_thing:
	; Here we set up paging
	mov ebx, PML4

	mov edi, PML4
	mov eax, PDPT0
	or eax, 0x03
	mov [edi], eax

	mov edi, PDPT0
	mov eax, PDT0
	or eax, 0x03
	mov [edi], eax

	mov edi, PDT0
	mov ecx, 0
	mov eax, PT0x4
	or eax, 0x03

	again:
		mov [edi], eax

		add eax, 0x1000
		add edi, 8

		inc ecx
		cmp ecx, 4
		jl again
		
	mov edi, PT0x4
	mov ecx, 0
	mov eax, 0x03

	again_pt:
		mov [edi], eax

		add eax, 0x1000
		add edi, 8

		inc ecx
		cmp ecx, STARTING_PAGE_AMOUNT
		jl again_pt

	; Enable PAE
	mov eax, cr4
    or eax, 1 << 5
    mov cr4, eax

	; Make cr3 point to our PML4
	mov eax, PML4
	mov cr3, eax

	; Setting the long mode bit
	mov ecx, 0xC0000080
	rdmsr
	or eax, 1 << 8
	wrmsr

	;xchg bx, bx

	; Actually enabling paging
	mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

	pop edi
	pop esi
	pop edx
	pop ecx
	pop ebx
	pop eax

	push dword 0
	push ebx

	lgdt [GDT64.Pointer]         ; Load the 64-bit global descriptor table.
    jmp GDT64.Code:Realm64       ; Set the code segment and enter 64-bit long mode.

[BITS 64]
extern kernel_stack
Realm64:
	; Here we set out not-code segments
	mov ax, GDT64.Data
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	pop rdi

	mov rsp, kernel_stack + 0x2000
	mov rbp, kernel_stack + 0x2000

	jmp kernel_entry

SECTION .bss
ALIGN 0x1000
global PML4
PML4:
	resb 0x1000

ALIGN 0x1000
PDPT0:
	resb 0x1000

ALIGN 0x1000
PDT0:
	resb 0x1000

ALIGN 0x1000
PT0x4:
	resb STARTING_PAGE_AMOUNT

SECTION .rodata
GDT64:                           ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64         ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 1                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64         ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64         ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .UserCode: equ $ - GDT64     ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11111010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .UserData: equ $ - GDT64     ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11110010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.