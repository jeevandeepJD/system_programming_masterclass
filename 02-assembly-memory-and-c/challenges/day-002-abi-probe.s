/*
 * Day 2 challenge A — hand-written routines that expose the ABI contract.
 *
 * Assembled by GNU as through GCC, in AT&T syntax:
 *
 *   gcc -std=c17 -Wall -Wextra -O0 -g -fno-omit-frame-pointer \
 *     day-002-abi-probe.s day-002-stack-frames-lab.c \
 *     -o /tmp/day-002-stack-frames-lab
 *
 * Nothing here is magic. Every routine is a deliberate, minimal example of
 * one rule from the System V AMD64 ABI:
 *
 *   - where the seventh integer argument lives,
 *   - what rsp looks like on entry,
 *   - what happens when a callee honours or ignores the callee-saved list.
 */

        .text

/* ------------------------------------------------------------------ */
/* unsigned long asm_read_rsp(void)                                    */
/*                                                                     */
/* Returns the value of rsp as seen *inside* the callee, immediately   */
/* after the call instruction pushed the return address.               */
/*                                                                     */
/* Predict the low four bits before you run it.                        */
/* ------------------------------------------------------------------ */
        .globl  asm_read_rsp
        .type   asm_read_rsp, @function
asm_read_rsp:
        movq    %rsp, %rax
        ret
        .size   asm_read_rsp, .-asm_read_rsp

/* ------------------------------------------------------------------ */
/* long asm_seven(long a1, ..., long a7)                               */
/*                                                                     */
/* Sums seven arguments. Six arrive in registers; the seventh was      */
/* pushed by the caller, so on entry the stack looks like              */
/*                                                                     */
/*   (%rsp)    return address                                          */
/*   8(%rsp)   a7                                                      */
/*                                                                     */
/* This routine builds no frame of its own, so 8(%rsp) stays valid for */
/* its whole body.                                                     */
/* ------------------------------------------------------------------ */
        .globl  asm_seven
        .type   asm_seven, @function
asm_seven:
        movq    %rdi, %rax              /* a1 */
        addq    %rsi, %rax              /* a2 */
        addq    %rdx, %rax              /* a3 */
        addq    %rcx, %rax              /* a4 */
        addq    %r8,  %rax              /* a5 */
        addq    %r9,  %rax              /* a6 */
        addq    8(%rsp), %rax           /* a7, from the caller's stack area */
        ret
        .size   asm_seven, .-asm_seven

/* ------------------------------------------------------------------ */
/* long asm_sum_squares(long n)                                        */
/*                                                                     */
/* Returns 1*1 + 2*2 + ... + n*n. It wants a second long-lived         */
/* register, so it borrows rbx — and therefore saves and restores it,  */
/* exactly as the ABI requires of a callee.                            */
/* ------------------------------------------------------------------ */
        .globl  asm_sum_squares
        .type   asm_sum_squares, @function
asm_sum_squares:
        pushq   %rbx                    /* preserve the caller's rbx        */
        xorl    %eax, %eax              /* total = 0                        */
        movq    $1, %rbx                /* i = 1                            */
.Lsq_test:
        cmpq    %rdi, %rbx
        jg      .Lsq_done
        movq    %rbx, %rcx
        imulq   %rbx, %rcx              /* rcx = i * i                      */
        addq    %rcx, %rax
        incq    %rbx
        jmp     .Lsq_test
.Lsq_done:
        popq    %rbx                    /* hand rbx back unchanged          */
        ret
        .size   asm_sum_squares, .-asm_sum_squares

/* ------------------------------------------------------------------ */
/* long asm_bad_sum_squares(long n)                                    */
/*                                                                     */
/* Identical arithmetic, identical return value, one missing pair of   */
/* instructions. It computes the right answer and still breaks its     */
/* caller. This is what an ABI violation actually looks like: not a    */
/* crash at the point of the mistake, but wrong data somewhere else.   */
/* ------------------------------------------------------------------ */
        .globl  asm_bad_sum_squares
        .type   asm_bad_sum_squares, @function
asm_bad_sum_squares:
        xorl    %eax, %eax
        movq    $1, %rbx                /* clobbers the caller's rbx        */
.Lbad_test:
        cmpq    %rdi, %rbx
        jg      .Lbad_done
        movq    %rbx, %rcx
        imulq   %rbx, %rcx
        addq    %rcx, %rax
        incq    %rbx
        jmp     .Lbad_test
.Lbad_done:
        ret
        .size   asm_bad_sum_squares, .-asm_bad_sum_squares

/* ------------------------------------------------------------------ */
/* long asm_probe_rbx(long (*fn)(long), long arg)                      */
/*                                                                     */
/* Puts a recognizable marker in rbx, calls fn(arg), and returns       */
/* whatever rbx holds afterwards. A well-behaved callee leaves the     */
/* marker intact; a careless one does not.                             */
/*                                                                     */
/* Three pushes are not decoration. On entry rsp is 8 modulo 16        */
/* because of the return address; three 8-byte pushes bring it back to */
/* 0 modulo 16, which is what the ABI requires at the next call.       */
/* ------------------------------------------------------------------ */
        .globl  asm_probe_rbx
        .type   asm_probe_rbx, @function
asm_probe_rbx:
        pushq   %rbx
        pushq   %rbp
        pushq   %r12
        movq    %rdi, %r12              /* keep the function pointer safe   */
        movq    %rsi, %rdi              /* arg becomes the callee's a1      */
        movabsq $0x5a5a5a5a5a5a5a5a, %rbx
        call    *%r12
        movq    %rbx, %rax              /* report rbx, not the callee's rax */
        popq    %r12
        popq    %rbp
        popq    %rbx
        ret
        .size   asm_probe_rbx, .-asm_probe_rbx

        .section .note.GNU-stack,"",@progbits
