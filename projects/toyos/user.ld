ENTRY(_user_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x40000000 + SIZEOF_HEADERS;
    .text ALIGN(4K) : {
        *(.text .text.*)
    } :text
    .rodata ALIGN(16) : {
        *(.rodata .rodata.*)
    } :text

    .data ALIGN(4K) : {
        *(.data .data.*)
    } :data
    .bss ALIGN(16) (NOLOAD) : {
        *(.bss .bss.*)
    } :data
}
