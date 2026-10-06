#ifndef TOYOS_H
#define TOYOS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PAGE_SIZE 4096ULL
#define ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))

struct interrupt_frame {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rdi, rsi, rbp, rdx, rcx, rbx, rax;
    uint64_t vector, error, rip, cs, rflags;
};

struct context {
    uint64_t r15, r14, r13, r12, rbx, rbp, rsp, rip;
};

extern uint8_t __kernel_start[];
extern uint8_t __kernel_end[];
extern uint8_t bootstrap_pml4[];
extern void (*isr_stub_table[32])(void);

void serial_init(void);
void serial_putc(char c);
void serial_write(const char *text);
void serial_hex(uint64_t value);
void serial_dec(uint64_t value);
void qemu_exit(uint8_t code) __attribute__((noreturn));
void panic_at(const char *message, const char *file, unsigned line)
    __attribute__((noreturn));

#define PANIC(message) panic_at((message), __FILE__, __LINE__)
#define ASSERT(condition) \
    do { if (!(condition)) { PANIC("assertion failed: " #condition); } } while (0)

void arch_init(void);
void interrupt_handler(struct interrupt_frame *frame);
void interrupts_enable(void);
uint64_t timer_ticks(void);

#if CONFIG_MILESTONE >= 7
void memory_init(uint64_t multiboot_info);
uint64_t frame_alloc(void);
void frame_free(uint64_t physical);
bool map_page(uint64_t virtual_address, uint64_t physical, uint64_t flags);
void *kalloc(size_t bytes);

void tasks_init(void);
void tasks_start(void) __attribute__((noreturn));
void scheduler_tick(void);
void context_switch(struct context *old_context, struct context *new_context);
#endif

#endif
