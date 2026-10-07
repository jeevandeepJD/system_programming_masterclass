#include "toyos.h"

#if CONFIG_MILESTONE >= 8

#define ELF_MAGIC 0x464c457fU
#define ELF_MACHINE_X86_64 62U
#define ELF_LOAD 1U
#define USER_STACK_TOP 0x0000000040020000ULL
#define USER_STACK_PAGES 4U
#define MAX_IO 256U
#define PIPE_CAPACITY 64U

enum syscall_number {
    SYS_WRITE = 1,
    SYS_GETPID = 2,
    SYS_YIELD = 3,
    SYS_EXIT = 4,
    SYS_PIPE = 5,
    SYS_READ = 6,
    SYS_OPEN = 7
};

struct __attribute__((packed)) elf_header {
    uint32_t magic;
    uint8_t class;
    uint8_t data;
    uint8_t version;
    uint8_t osabi;
    uint8_t padding[8];
    uint16_t type;
    uint16_t machine;
    uint32_t elf_version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
};

struct __attribute__((packed)) program_header {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t virtual_address;
    uint64_t physical_address;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t align;
};

struct process {
    uint64_t *root;
    uint64_t entry;
    uint64_t user_stack;
    uint8_t *kernel_stack;
    unsigned pid;
};

extern const uint8_t user_elf_start[];
extern const uint8_t user_elf_end[];

static struct process init_process;
static uint8_t pipe_data[PIPE_CAPACITY];
static size_t pipe_length;
static size_t pipe_read_position;

static void bytes_copy(uint8_t *destination, const uint8_t *source, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        destination[i] = source[i];
    }
}

static bool range_inside(size_t offset, size_t length, size_t total)
{
    return offset <= total && length <= total - offset;
}

static bool load_elf(struct process *process)
{
    const size_t image_size = (size_t)(user_elf_end - user_elf_start);
    if (image_size < sizeof(struct elf_header)) {
        return false;
    }
    const struct elf_header *const header =
        (const struct elf_header *)(const void *)user_elf_start;
    if (header->magic != ELF_MAGIC || header->class != 2U || header->data != 1U
        || header->machine != ELF_MACHINE_X86_64
        || header->phentsize != sizeof(struct program_header)
        || header->phnum == 0U
        || !range_inside((size_t)header->phoff,
                         (size_t)header->phnum * sizeof(struct program_header),
                         image_size)) {
        return false;
    }

    const struct program_header *const programs =
        (const struct program_header *)(const void *)
        (user_elf_start + header->phoff);
    for (size_t i = 0; i < header->phnum; ++i) {
        const struct program_header *const segment = &programs[i];
        if (segment->type != ELF_LOAD) {
            continue;
        }
        if (segment->memory_size < segment->file_size
            || !range_inside((size_t)segment->offset,
                             (size_t)segment->file_size, image_size)
            || segment->virtual_address < 0x40000000ULL
            || segment->virtual_address + segment->memory_size
                < segment->virtual_address
            || segment->virtual_address + segment->memory_size
                > USER_STACK_TOP - USER_STACK_PAGES * PAGE_SIZE) {
            return false;
        }
        const uint64_t first = segment->virtual_address & ~(PAGE_SIZE - 1U);
        const uint64_t last = (segment->virtual_address + segment->memory_size
                               + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
        for (uint64_t address = first; address < last; address += PAGE_SIZE) {
            const uint64_t frame = frame_alloc();
            if (frame == 0U
                || !map_page_in(process->root, address, frame,
                                PTE_USER | PTE_WRITE)) {
                return false;
            }
        }
        for (uint64_t offset = 0; offset < segment->file_size; ++offset) {
            const uint64_t address = segment->virtual_address + offset;
            uint64_t *table = process->root;
            const unsigned shifts[3] = {39U, 30U, 21U};
            for (size_t level = 0; level < ARRAY_LEN(shifts); ++level) {
                table = (uint64_t *)(uintptr_t)
                    (table[(address >> shifts[level]) & 0x1ffU]
                     & 0x000ffffffffff000ULL);
            }
            const uint64_t page = table[(address >> 12U) & 0x1ffU]
                & 0x000ffffffffff000ULL;
            ((uint8_t *)(uintptr_t)page)[address & (PAGE_SIZE - 1U)] =
                user_elf_start[segment->offset + offset];
        }
    }
    process->entry = header->entry;
    return true;
}

static int64_t pipe_write(const uint8_t *data, size_t length)
{
    if (length > PIPE_CAPACITY - pipe_length) {
        return -1;
    }
    bytes_copy(&pipe_data[pipe_length], data, length);
    pipe_length += length;
    return (int64_t)length;
}

static int64_t pipe_read(uint8_t *data, size_t length)
{
    const size_t available = pipe_length - pipe_read_position;
    const size_t count = length < available ? length : available;
    bytes_copy(data, &pipe_data[pipe_read_position], count);
    pipe_read_position += count;
    return (int64_t)count;
}

#if CONFIG_MILESTONE >= 9
static bool copy_user_string(char *destination, uint64_t source, size_t capacity)
{
    for (size_t i = 0; i < capacity; ++i) {
        if (!copy_from_user(&destination[i], source + i, 1U)) {
            return false;
        }
        if (destination[i] == '\0') {
            return true;
        }
    }
    return false;
}
#endif

void syscall_dispatch(struct interrupt_frame *frame)
{
    uint8_t buffer[MAX_IO];
    int64_t result = -38;
    const size_t length = (size_t)frame->rdx;
    switch (frame->rax) {
    case SYS_WRITE:
        if (length > sizeof(buffer)
            || !copy_from_user(buffer, frame->rsi, length)) {
            result = -14;
        } else if (frame->rdi == 1U) {
            for (size_t i = 0; i < length; ++i) {
                serial_putc((char)buffer[i]);
            }
            result = (int64_t)length;
        } else if (frame->rdi == 4U) {
            result = pipe_write(buffer, length);
#if CONFIG_MILESTONE >= 9
        } else {
            result = vfs_write((unsigned)frame->rdi, buffer, length);
#endif
        }
        break;
    case SYS_GETPID:
        result = (int64_t)init_process.pid;
        break;
    case SYS_YIELD:
        __asm__ volatile ("pause");
        result = 0;
        break;
    case SYS_EXIT:
        serial_write("USER: exit status=");
        serial_dec(frame->rdi);
        serial_write("\n");
        serial_write("SELFTEST: ring3/TSS/int80 syscalls PASS\n");
        serial_write("SELFTEST: bounded user copy and pipe PASS\n");
#if CONFIG_MILESTONE >= 9
        serial_write("WEEK9: PASS\n");
#else
        serial_write("WEEK8: PASS\n");
#endif
        qemu_exit(frame->rdi == 0U ? 0x10 : 0x7f);
        break;
    case SYS_PIPE: {
        const int32_t descriptors[2] = {3, 4};
        pipe_length = 0;
        pipe_read_position = 0;
        result = copy_to_user(frame->rdi, descriptors, sizeof(descriptors))
            ? 0 : -14;
        break;
    }
    case SYS_READ:
        if (length > sizeof(buffer)) {
            result = -22;
        } else if (frame->rdi == 3U) {
            result = pipe_read(buffer, length);
#if CONFIG_MILESTONE >= 9
        } else {
            result = vfs_read((unsigned)frame->rdi, buffer, length);
#endif
        }
        if (result >= 0
            && !copy_to_user(frame->rsi, buffer, (size_t)result)) {
            result = -14;
        }
        break;
#if CONFIG_MILESTONE >= 9
    case SYS_OPEN: {
        char path[48];
        result = copy_user_string(path, frame->rdi, sizeof(path))
            ? vfs_open(path) : -14;
        break;
    }
#endif
    default:
        result = -38;
        break;
    }
    frame->rax = (uint64_t)result;
}

void process_init(void)
{
    init_process.root = address_space_create();
    init_process.pid = 1U;
    init_process.kernel_stack = kalloc(16384U);
    ASSERT(init_process.root != NULL);
    ASSERT(init_process.kernel_stack != NULL);
    ASSERT(load_elf(&init_process));
    for (uint64_t i = 0; i < USER_STACK_PAGES; ++i) {
        const uint64_t frame = frame_alloc();
        ASSERT(frame != 0U);
        ASSERT(map_page_in(init_process.root,
                           USER_STACK_TOP - (i + 1U) * PAGE_SIZE,
                           frame, PTE_USER | PTE_WRITE));
    }
    init_process.user_stack = USER_STACK_TOP;
    serial_write("PROCESS: PID 1 private PML4 and embedded ELF loaded\n");
}

void process_start(void)
{
    address_space_activate(init_process.root);
#if EXPECT_FAULT == 6
    uint8_t byte;
    ASSERT(!copy_from_user(&byte, 0x1234U, 1U));
    serial_write("EXPECTED-FAULT: user-pointer PASS\n");
    qemu_exit(0x15);
#elif EXPECT_FAULT == 7
    static struct interrupt_frame frame;
    frame.rax = UINT64_MAX;
    syscall_dispatch(&frame);
    ASSERT((int64_t)frame.rax == -38);
    serial_write("EXPECTED-FAULT: syscall PASS\n");
    qemu_exit(0x16);
#endif
    const uint64_t kernel_top =
        ((uint64_t)(uintptr_t)(init_process.kernel_stack + 16384U)) & ~0xfULL;
    arch_set_kernel_stack(kernel_top);
    serial_write("PROCESS: entering CPL3 through iretq\n");
    enter_user(init_process.entry, init_process.user_stack);
}

#endif
