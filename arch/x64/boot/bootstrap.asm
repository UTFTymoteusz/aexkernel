[BITS 32]
SECTION .bootstrap

STARTING_PAGE_AMOUNT equ 1600
PAGE_FLAGS           equ 0x03

global _start
_start:
	jmp bootstrap

ALIGN 4
mboot:
	MULTIBOOT_PAGE_ALIGN	equ 1 << 0
	MULTIBOOT_MEMORY_INFO	equ 1 << 1
	MULTIBOOT_GRAPHICS_INFO	equ 1 << 2
	MULTIBOOT_HEADER_MAGIC	equ 0x1BADB002
	MULTIBOOT_HEADER_FLAGS  equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_GRAPHICS_INFO
	MULTIBOOT_CHECKSUM	equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)

	dd MULTIBOOT_HEADER_MAGIC
	dd MULTIBOOT_HEADER_FLAGS
	dd MULTIBOOT_CHECKSUM

	dd 0
	dd 0
	dd 0
	dd 0
	dd 0

	; Graphics things
	dd 0
	dd 1280
	dd 720
	dd 32

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
	or eax, PAGE_FLAGS
	mov [edi], eax

	add edi, 8 * 511
	mov eax, PDPT1
	or eax, PAGE_FLAGS
	mov [edi], eax


	mov edi, PDPT0
	mov eax, PDT0
	or eax, PAGE_FLAGS
	mov [edi], eax

	mov edi, PDT0
	mov ecx, 0
	mov eax, PT0x4
	or eax, PAGE_FLAGS

	again0:
		mov [edi], eax

		add eax, 0x1000
		add edi, 8

		inc ecx
		cmp ecx, 4
		jl again0
		
	mov edi, PT0x4
	mov ecx, 0
	mov eax, PAGE_FLAGS

	add edi, 8
	add eax, 0x1000

	again_pt0:
		mov [edi], eax

		add eax, 0x1000
		add edi, 8

		inc ecx
		cmp ecx, STARTING_PAGE_AMOUNT - 1
		jl again_pt0


	mov edi, PDPT1
	mov eax, PDT1
	or eax, PAGE_FLAGS
	add edi, 8 * 510
	mov [edi], eax

	mov eax, PDT2
	or eax, PAGE_FLAGS
	add edi, 8
	mov [edi], eax
	

	mov edi, PDT1
	mov ecx, 0
	mov eax, PT1x4
	or eax, PAGE_FLAGS


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

	lgdt [GDT64init.Pointer]
    jmp GDT64init.Code:Realm64

[BITS 64]
extern kernel_stack
extern tss_ptr

extern kernel_main
Realm64:
	mov rdi, PDT1
	mov rcx, 0
	mov rax, PT1x4
	or rax, PAGE_FLAGS

	again1:
		mov [rdi], rax

		add rax, 0x1000
		add rdi, 8

		inc rcx
		cmp rcx, 4
		jl again1
		
	mov rdi, PT1x4
	mov rcx, 0
	mov rax, PAGE_FLAGS

	again_pt1:
		mov [rdi], rax

		add rax, 0x1000
		add rdi, 8

		inc rcx
		cmp rcx, STARTING_PAGE_AMOUNT
		jl again_pt1
	
	mov rax, cr3
	mov cr3, rax

	; Here we set out not-code segments
	mov ax, GDT64.Data
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	mov ss, ax

	pop rdi

	mov rsp, kernel_stack + 0x8000
	;mov rbp, kernel_stack + 0x8000

	mov rax, GDT64
	add rax, GDT64.TSS
	mov rbx, tss_ptr

	mov word [rax], 0xFFFF
	add rax, 2

	mov rcx, rbx
	and rcx, 0xFFFF
	mov word [rax], cx

	add rax, 2
	mov rcx, rbx
	shr rcx, 16
	and rcx, 0xFF
	mov byte [rax], cl

	add rax, 3
	mov rcx, rbx
	shr rcx, 24
	and rcx, 0xFF
	mov byte [rax], cl

	add rax, 1
	mov rcx, rbx
	shr rcx, 32
	and rcx, 0xFFFFFFFF
	mov dword [rax], ecx

	;add rax, 1
	;mov dword [rax], 0xFFFFFFFF

	; Here we load our actual GDT
	lgdt [GDT64.Pointer]

	; And the TSS
	mov ax, GDT64.TSS

	;xchg bx, bx
	ltr ax
	mov rbp, 0

	; enabling SSE, skipping checks because AMD64 mandates SSE and SSE2 itself (thank god)
	mov rax, cr0
	and ax, 0xFFFB ; clear coprocessor emulation flag
	or  ax, 0x0002 ; set coprocessor monitoring flag
	mov cr0, rax

	mov rax, cr4
	or ax, 0x600 ; set OSFXSR and OSXMMEXCPT
	mov cr4, rax

	call kernel_main

	halt:
		hlt
		jmp halt

; This is our temporal bootstrap GDT
GDT64init:                       ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64init     ; The null descriptor.
    dw 0xFFFF                    ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 0                         ; Access.
    db 1                         ; Granularity.
    db 0                         ; Base (high).
    .Code: equ $ - GDT64init     ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10011010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .Data: equ $ - GDT64init     ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 10010010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64init - 1         ; Limit.
    dq GDT64init                 ; Base.

SECTION .data
; This bigbong is our glorious GDT thats in the higher half
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
    .UserData: equ $ - GDT64     ; The data descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11110010b                 ; Access (read/write).
    db 00000000b                 ; Granularity.
    db 0                         ; Base (high).
    .UserCode: equ $ - GDT64     ; The code descriptor.
    dw 0                         ; Limit (low).
    dw 0                         ; Base (low).
    db 0                         ; Base (middle)
    db 11111010b                 ; Access (exec/read).
    db 10101111b                 ; Granularity, 64 bits flag, limit19:16.
    db 0                         ; Base (high).
    .TSS: equ $ - GDT64     	 ; TSS boi
	dw 0
	dw 0
	db 0
	db 11101001b
	db 10100000b
	db 0
    dd 0
    dd 0
    .Pointer:                    ; The GDT-pointer.
    dw $ - GDT64 - 1             ; Limit.
    dq GDT64                     ; Base.

SECTION .bootstrap.bss

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
	resq STARTING_PAGE_AMOUNT


ALIGN 0x1000
global PDPT1
PDPT1:
	resb 0x1000

global PDT1
ALIGN 0x1000
PDT1:
	resb 0x1000

ALIGN 0x1000
PT1x4:
	resq STARTING_PAGE_AMOUNT

global PDT2
ALIGN 0x1000
PDT2:
	resb 0x1000