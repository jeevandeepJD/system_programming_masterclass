#if CONFIG_MILESTONE >= 8
.section .rodata.user_image, "a"
.align 8
.global user_elf_start
.global user_elf_end
user_elf_start:
    .incbin USER_IMAGE
user_elf_end:
#endif

.section .note.GNU-stack, "", @progbits
