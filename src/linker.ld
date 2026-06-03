ENTRY(_start)
SECTIONS
{
    . = 0x100000;
    .multiboot ALIGN(8) : { *(.multiboot) }
    . = ALIGN(4);
    .text : { *(.text*) }
    . = ALIGN(4);
    .rodata : { *(.rodata*) }
    . = ALIGN(4);
    .eh_frame : { *(.eh_frame) }
    . = ALIGN(4);
    .data : { *(.data*) }
    . = ALIGN(4);
    .bss (NOLOAD) :
    {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    }
    . = ALIGN(16);
    .bootstrap_stack (NOLOAD) : {
        *(.bootstrap_stack)
    }
    _end = .;
}
