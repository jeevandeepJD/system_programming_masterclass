.section .text
.code64
.global _user_start
.type _user_start, @function
_user_start:
    movq $1, %rax
    movq $1, %rdi
    leaq hello(%rip), %rsi
    movq $hello_end-hello, %rdx
    int $0x80

    movq $2, %rax
    int $0x80

    movq $3, %rax
    int $0x80

    movq $5, %rax
    leaq pipe_fds(%rip), %rdi
    int $0x80

    movq $1, %rax
    movslq pipe_fds+4(%rip), %rdi
    leaq pipe_message(%rip), %rsi
    movq $pipe_message_end-pipe_message, %rdx
    int $0x80

    movq $6, %rax
    movslq pipe_fds(%rip), %rdi
    leaq io_buffer(%rip), %rsi
    movq $pipe_message_end-pipe_message, %rdx
    int $0x80
    movq %rax, %rdx
    movq $1, %rax
    movq $1, %rdi
    leaq io_buffer(%rip), %rsi
    int $0x80

#if CONFIG_MILESTONE >= 9
    movq $7, %rax
    leaq ramfs_path(%rip), %rdi
    int $0x80
    movq %rax, %rdi
    movq $6, %rax
    leaq io_buffer(%rip), %rsi
    movq $64, %rdx
    int $0x80
    movq %rax, %rdx
    movq $1, %rax
    movq $1, %rdi
    leaq io_buffer(%rip), %rsi
    int $0x80

    movq $7, %rax
    leaq ramfs_note_path(%rip), %rdi
    int $0x80
    movq %rax, %rdi
    movq $1, %rax
    leaq ramfs_write_message(%rip), %rsi
    movq $ramfs_write_end-ramfs_write_message, %rdx
    int $0x80
#endif

    movq $4, %rax
    xorq %rdi, %rdi
    int $0x80
1:
    pause
    jmp 1b
.size _user_start, . - _user_start

.section .rodata
hello:
    .ascii "USER: write/getpid/yield active\n"
hello_end:
pipe_message:
    .ascii "USER: bounded pipe round-trip\n"
pipe_message_end:
#if CONFIG_MILESTONE >= 9
ramfs_path:
    .asciz "/hello"
ramfs_note_path:
    .asciz "/note"
ramfs_write_message:
    .ascii "user-fd-write"
ramfs_write_end:
#endif

.section .data
.align 8
pipe_fds:
    .long 0, 0
io_buffer:
    .skip 128

.section .note.GNU-stack, "", @progbits
