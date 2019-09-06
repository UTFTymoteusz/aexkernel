ENTRY(_start)
OUTPUT_FORMAT(elf64-little)

SECTIONS
{
	. = 1M;

    .text BLOCK(4K) : ALIGN(4K)
    {
        _start_text = .;
        code = .;

        *(.bootstrap)
        *(.text)
        
        _end_text = .;
    }
	.rodata BLOCK(4K) : ALIGN(4K)
	{
		_start_rodata = .;
		*(.rodata)
		_end_rodata = .;
	}
	.data BLOCK(4K) : ALIGN(4K)
	{
		_start_data = .;
		data = .;
		*(.data)
		_end_data = .;
	}
	.bss BLOCK(4K) : ALIGN(4K)
	{
		_start_bss = .;
		bss = .;
		*(COMMON)
		*(.bss)
		_end_bss = .;
	}
	end = .;
}