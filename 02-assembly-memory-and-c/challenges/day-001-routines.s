/*
 * Week 2 Day 1: three small System V AMD64 routines.
 * GNU assembler, AT&T syntax. The learner extensions belong in the lesson;
 * this file supplies observable routines, not those extension answers.
 */
    .text

    .globl asm_sum_to
    .type asm_sum_to, @function
asm_sum_to:
    xorl %eax, %eax            /* sum = 0 */
    xorl %ecx, %ecx            /* i = 0 */
.Lsum_loop:
    cmpq %rdi, %rcx
    jge .Lsum_done
    addq %rcx, %rax
    incq %rcx
    jmp .Lsum_loop
.Lsum_done:
    ret
    .size asm_sum_to, .-asm_sum_to

    .globl asm_scale_add
    .type asm_scale_add, @function
asm_scale_add:
    /* long asm_scale_add(long value, long scale, long bias) */
    movq %rdi, %rax
    imulq %rsi, %rax
    addq %rdx, %rax
    ret
    .size asm_scale_add, .-asm_scale_add

    .globl asm_count_byte
    .type asm_count_byte, @function
asm_count_byte:
    /* size_t asm_count_byte(const unsigned char *, size_t, unsigned char) */
    xorl %eax, %eax            /* matches */
    xorl %ecx, %ecx            /* index */
.Lbyte_loop:
    cmpq %rsi, %rcx
    jae .Lbyte_done
    cmpb %dl, (%rdi,%rcx,1)
    jne .Lbyte_next
    incq %rax
.Lbyte_next:
    incq %rcx
    jmp .Lbyte_loop
.Lbyte_done:
    ret
    .size asm_count_byte, .-asm_count_byte

    .section .note.GNU-stack,"",@progbits
