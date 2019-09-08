ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
	KERNEL_VMA = 0xFFFFFFFF80000000;

	. = 1M;

    .bootstrap ALIGN(4K) : 
    {
		_start_text = .;
        *(.bootstrap)
		*(.bss.bootstrap)
	}

	. += KERNEL_VMA;
	. += 512K; /* find a way to get rid of this, this is not okay */

    .text ALIGN(4K) : AT(ADDR(.text) - KERNEL_VMA)
    {
        *(.text)
		_end_text = . - KERNEL_VMA;
    }

	.rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_VMA)
	{
		_start_rodata = . - KERNEL_VMA;
		*(.rodata)
		_end_rodata = . - KERNEL_VMA;
	}

	.data ALIGN(4K) : AT(ADDR(.data) - KERNEL_VMA)
	{
		_start_data = . - KERNEL_VMA;
		*(.data)
		_end_data = . - KERNEL_VMA;
	}

	.bss ALIGN(4K) : AT(ADDR(.bss) - KERNEL_VMA)
	{
		_start_bss = . - KERNEL_VMA;
		*(COMMON)
		*(.bss)
		_end_bss = . - KERNEL_VMA;
	}

	/DISCARD/ :
	{ 
		*(.note.gnu.build-id)
		/**(.comment)
		*(.stab)
		*(.stabstr)
		*(.eh_frame)
		*(.__GNU_EH_FRAME_HDR)
		*(.debug_loc)
		*(.debug_info)
		*(.debug_aranges)
		*(.debug_ranges)
		*(.debug_abbrev)
		*(.debug_line)
		*(.debug_str)*/
	}
}