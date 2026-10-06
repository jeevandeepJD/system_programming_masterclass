#include "toyos.h"

struct __attribute__((packed)) descriptor_pointer {
    uint16_t limit;
    uint64_t base;
};

struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t attributes;
    uint16_t offset_middle;
    uint32_t offset_high;
    uint32_t reserved;
};

struct __attribute__((packed)) tss64 {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iomap_base;
};

extern void irq0_stub(void);

static uint64_t gdt[5];
static struct tss64 tss;
static struct idt_entry idt[256];
static uint8_t interrupt_stack[16384] __attribute__((aligned(16)));
static volatile uint64_t ticks;

static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static inline void outw(uint16_t port, uint16_t value)
{
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

void serial_init(void)
{
    outb(0x3f8U + 1U, 0x00);
    outb(0x3f8U + 3U, 0x80);
    outb(0x3f8U + 0U, 0x01);
    outb(0x3f8U + 1U, 0x00);
    outb(0x3f8U + 3U, 0x03);
    outb(0x3f8U + 2U, 0xc7);
    outb(0x3f8U + 4U, 0x0b);
}

void serial_putc(char c)
{
    while ((inb(0x3f8U + 5U) & 0x20U) == 0U) {
    }
    outb(0x3f8U, (uint8_t)c);
}

void serial_write(const char *text)
{
    while (*text != '\0') {
        if (*text == '\n') {
            serial_putc('\r');
        }
        serial_putc(*text++);
    }
}

void serial_hex(uint64_t value)
{
    static const char digits[] = "0123456789abcdef";
    serial_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        serial_putc(digits[(value >> (unsigned)shift) & 0xfU]);
    }
}

void serial_dec(uint64_t value)
{
    char buffer[21];
    size_t position = sizeof(buffer);
    buffer[--position] = '\0';
    do {
        buffer[--position] = (char)('0' + (value % 10U));
        value /= 10U;
    } while (value != 0U);
    serial_write(&buffer[position]);
}

void qemu_exit(uint8_t code)
{
    outw(0x501, code);
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void panic_at(const char *message, const char *file, unsigned line)
{
    serial_write("PANIC: ");
    serial_write(message);
    serial_write(" at ");
    serial_write(file);
    serial_write(":");
    serial_dec(line);
    serial_write("\n");
#if EXPECT_FAULT == 3
    serial_write("EXPECTED-FAULT: assert PASS\n");
    qemu_exit(0x12);
#else
    qemu_exit(0x7f);
#endif
}

static void set_idt_gate(unsigned vector, void (*handler)(void), uint8_t ist)
{
    const uint64_t address = (uint64_t)(uintptr_t)handler;
    idt[vector].offset_low = (uint16_t)address;
    idt[vector].selector = 0x08;
    idt[vector].ist = ist;
    idt[vector].attributes = 0x8e;
    idt[vector].offset_middle = (uint16_t)(address >> 16U);
    idt[vector].offset_high = (uint32_t)(address >> 32U);
    idt[vector].reserved = 0;
}

static void gdt_init(void)
{
    const uint64_t base = (uint64_t)(uintptr_t)&tss;
    const uint64_t limit = sizeof(tss) - 1U;
    gdt[0] = 0;
    gdt[1] = 0x00af9a000000ffffULL;
    gdt[2] = 0x00cf92000000ffffULL;
    gdt[3] = (limit & 0xffffU) | ((base & 0xffffffU) << 16U)
        | (0x89ULL << 40U) | ((limit & 0xf0000U) << 32U)
        | ((base & 0xff000000U) << 32U);
    gdt[4] = base >> 32U;

    tss.rsp[0] = (uint64_t)(uintptr_t)(interrupt_stack + sizeof(interrupt_stack));
    tss.ist[0] = tss.rsp[0];
    tss.iomap_base = sizeof(tss);
    const struct descriptor_pointer pointer = {
        .limit = (uint16_t)(sizeof(gdt) - 1U),
        .base = (uint64_t)(uintptr_t)gdt
    };
    __asm__ volatile (
        "lgdt %0\n"
        "pushq $0x08\n"
        "leaq 1f(%%rip), %%rax\n"
        "pushq %%rax\n"
        "lretq\n"
        "1:\n"
        "movw $0x10, %%ax\n"
        "movw %%ax, %%ds\n"
        "movw %%ax, %%es\n"
        "movw %%ax, %%ss\n"
        "movw $0x18, %%ax\n"
        "ltr %%ax\n"
        : : "m"(pointer) : "rax", "memory"
    );
}

static void pic_and_pit_init(void)
{
#if CONFIG_MILESTONE >= 7
    outb(0x20, 0x11);
    outb(0xa0, 0x11);
    outb(0x21, 0x20);
    outb(0xa1, 0x28);
    outb(0x21, 0x04);
    outb(0xa1, 0x02);
    outb(0x21, 0x01);
    outb(0xa1, 0x01);
    outb(0x21, 0xfe);
    outb(0xa1, 0xff);

    const uint16_t divisor = 11932U;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)divisor);
    outb(0x40, (uint8_t)(divisor >> 8U));
#endif
}

void arch_init(void)
{
    gdt_init();
    for (unsigned i = 0; i < 32U; ++i) {
        const uint8_t ist = (i == 2U || i == 8U || i == 14U) ? 1U : 0U;
        set_idt_gate(i, isr_stub_table[i], ist);
    }
    set_idt_gate(32, irq0_stub, 0);
    const struct descriptor_pointer pointer = {
        .limit = (uint16_t)(sizeof(idt) - 1U),
        .base = (uint64_t)(uintptr_t)idt
    };
    __asm__ volatile ("lidt %0" : : "m"(pointer));
    pic_and_pit_init();
}

void interrupts_enable(void)
{
    __asm__ volatile ("sti");
}

uint64_t timer_ticks(void)
{
    return ticks;
}

void interrupt_handler(struct interrupt_frame *frame)
{
    static const char *const names[32] = {
        "divide error", "debug", "NMI", "breakpoint", "overflow",
        "bound range", "invalid opcode", "device unavailable", "double fault",
        "coprocessor overrun", "invalid TSS", "segment not present",
        "stack fault", "general protection", "page fault", "reserved",
        "x87 floating point", "alignment check", "machine check", "SIMD",
        "virtualization", "control protection", "reserved", "reserved",
        "reserved", "reserved", "reserved", "reserved", "hypervisor",
        "VMM communication", "security", "reserved"
    };

    if (frame->vector == 32U) {
        ++ticks;
        outb(0x20, 0x20);
#if CONFIG_MILESTONE >= 7
        scheduler_tick();
#endif
        return;
    }

    if (frame->vector == 3U) {
        serial_write("SELFTEST: IDT breakpoint frame PASS\n");
        return;
    }

    serial_write("EXCEPTION ");
    serial_dec(frame->vector);
    serial_write(" (");
    serial_write(frame->vector < 32U ? names[frame->vector] : "unknown");
    serial_write(") error=");
    serial_hex(frame->error);
    serial_write(" rip=");
    serial_hex(frame->rip);
    serial_write("\n");

    if (frame->vector == 14U) {
        uint64_t address;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(address));
        serial_write("PAGE-FAULT address=");
        serial_hex(address);
        serial_write(" flags=");
        serial_write((frame->error & 1U) != 0U ? "protection " : "not-present ");
        serial_write((frame->error & 2U) != 0U ? "write " : "read ");
        serial_write((frame->error & 4U) != 0U ? "user " : "supervisor ");
        serial_write((frame->error & 8U) != 0U ? "reserved-bit " : "");
        serial_write((frame->error & 16U) != 0U ? "instruction-fetch" : "");
        serial_write("\n");
    }

#if EXPECT_FAULT == 1
    if (frame->vector == 0U) {
        serial_write("EXPECTED-FAULT: divide PASS\n");
        qemu_exit(0x11);
    }
#elif EXPECT_FAULT == 2
    if (frame->vector == 14U) {
        serial_write("EXPECTED-FAULT: page PASS\n");
        qemu_exit(0x11);
    }
#endif
    PANIC("unexpected exception");
}
