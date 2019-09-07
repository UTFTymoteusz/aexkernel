ENTRY(_start)
OUTPUT_FORMAT(elf64-little)

SECTIONS
{
	KERNEL_VMA = 0xC0000000000;

	. = 1M;

    .bootstrap :
    {
        *(.bootstrap)

		. = BLOCK(4K);
		. = ALIGN(4K);
	}

	/*. = KERNEL_VMA;*/

	_start_text = .;
	code = .;
    .text : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text)

		. = BLOCK(4K);
		. = ALIGN(4K);
    }
	_end_text = .;

	_start_rodata = .;
	.rodata : AT(ADDR(.rodata) - KERNEL_VMA)
	{
		*(.rodata)

		. = BLOCK(4K);
		. = ALIGN(4K);
	}
	_end_rodata = .;

	_start_data = .;
	data = .;
	.data : AT(ADDR(.data) - KERNEL_VMA)
	{
		*(.data)

		. = BLOCK(4K);
		. = ALIGN(4K);
	}
	_end_data = .;

	_start_bss = .;
	bss = .;
	.bss : AT(ADDR(.bss) - KERNEL_VMA)
	{
		*(COMMON)
		*(.bss)

		. = BLOCK(4K);
		. = ALIGN(4K);
	}
	_end_bss = .;
	
	end = .;
}