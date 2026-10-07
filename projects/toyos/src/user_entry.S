#if CONFIG_MILESTONE >= 8
.section .text
.code64
.global enter_user
.type enter_user, @function
enter_user:
    cli
    movw $0x1b, %ax
    movw %ax, %ds
    movw %ax, %es
    pushq $0x1b
    pushq %rsi
    pushfq
    orq $0x200, (%rsp)
    pushq $0x23
    pushq %rdi
    iretq
.size enter_user, . - enter_user
#endif

.section .note.GNU-stack, "", @progbits
