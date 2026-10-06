#include "toyos.h"

#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289U

static void run_requested_fault(void)
{
#if EXPECT_FAULT == 1
    serial_write("INJECT: divide error\n");
    __asm__ volatile (
        "xor %%edx, %%edx\n"
        "mov $1, %%eax\n"
        "xor %%ecx, %%ecx\n"
        "div %%ecx\n"
        : : : "rax", "rcx", "rdx"
    );
#elif EXPECT_FAULT == 2
    serial_write("INJECT: unmapped page write\n");
    *(volatile uint64_t *)(uintptr_t)0x70000000ULL = 1;
#elif EXPECT_FAULT == 3
    serial_write("INJECT: assertion failure\n");
    ASSERT(false);
#endif
}

void kernel_main(uint32_t magic, uint32_t multiboot_info)
{
    serial_init();
    serial_write("\nToyOS x86-64 milestone ");
    serial_dec(CONFIG_MILESTONE);
    serial_write("\n");
    ASSERT(magic == MULTIBOOT2_BOOTLOADER_MAGIC);
    ASSERT(multiboot_info != 0U);

    arch_init();
    serial_write("BOOT: Multiboot2 -> 32-bit entry -> long mode PASS\n");
    serial_write("SELFTEST: GDT/TSS/IST installed PASS\n");
    __asm__ volatile ("int3");
    run_requested_fault();

#if CONFIG_MILESTONE >= 7
    memory_init(multiboot_info);
    tasks_init();
    tasks_start();
#else
    serial_write("WEEK6: PASS\n");
    qemu_exit(0x10);
#endif
}
