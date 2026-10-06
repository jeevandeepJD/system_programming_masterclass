.set MB2_MAGIC, 0xe85250d6
.set MB2_ARCH, 0
.set MB2_LENGTH, mb2_end - mb2_start
.set MB2_CHECKSUM, -(MB2_MAGIC + MB2_ARCH + MB2_LENGTH)

.section .multiboot2, "a"
.align 8
mb2_start:
    .long MB2_MAGIC
    .long MB2_ARCH
    .long MB2_LENGTH
    .long MB2_CHECKSUM
    .short 0
    .short 0
    .long 8
mb2_end:

.section .text.boot, "ax"
.code32
.global _start
.type _start, @function
_start:
    cli
    movl %eax, boot_magic
    movl %ebx, boot_info
    movl $boot_stack_top, %esp

    movl $bootstrap_pdpt, %eax
    orl $3, %eax
    movl %eax, bootstrap_pml4
    movl $bootstrap_pd, %eax
    orl $3, %eax
    movl %eax, bootstrap_pdpt

    xorl %ecx, %ecx
1:
    movl %ecx, %eax
    shll $21, %eax
    orl $0x83, %eax
    movl %eax, bootstrap_pd(,%ecx,8)
    movl $0, bootstrap_pd+4(,%ecx,8)
    incl %ecx
    cmpl $512, %ecx
    jne 1b

    movl %cr4, %eax
    orl $(1 << 5), %eax
    movl %eax, %cr4
    movl $bootstrap_pml4, %eax
    movl %eax, %cr3

    movl $0xc0000080, %ecx
    rdmsr
    orl $(1 << 8), %eax
    wrmsr

    movl %cr0, %eax
    orl $(1 << 31), %eax
    movl %eax, %cr0
    lgdt boot_gdt_pointer
    ljmp $0x08, $long_mode_entry

.code64
long_mode_entry:
    movw $0x10, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss
    xorw %ax, %ax
    movw %ax, %fs
    movw %ax, %gs
    movq $boot_stack_top, %rsp
    xorq %rbp, %rbp
    movl boot_magic(%rip), %edi
    movl boot_info(%rip), %esi
    call kernel_main
2:
    cli
    hlt
    jmp 2b

.align 8
boot_gdt:
    .quad 0
    .quad 0x00af9a000000ffff
    .quad 0x00cf92000000ffff
boot_gdt_end:
boot_gdt_pointer:
    .word boot_gdt_end - boot_gdt - 1
    .long boot_gdt

.section .bss, "aw", @nobits
.align 4096
.global bootstrap_pml4
bootstrap_pml4:
    .skip 4096
.global bootstrap_pdpt
bootstrap_pdpt:
    .skip 4096
.global bootstrap_pd
bootstrap_pd:
    .skip 4096
.align 16
boot_stack:
    .skip 32768
boot_stack_top:
.align 4
boot_magic:
    .skip 4
boot_info:
    .skip 4

.section .note.GNU-stack, "", @progbits
