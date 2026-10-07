#include "toyos.h"

#if CONFIG_MILESTONE >= 7

#define MAX_PHYSICAL_MEMORY (128ULL * 1024ULL * 1024ULL)
#define FRAME_COUNT (MAX_PHYSICAL_MEMORY / PAGE_SIZE)
#define BITMAP_WORDS (FRAME_COUNT / 64ULL)
#define HEAP_BASE 0x50000000ULL
#define HEAP_PAGES 8ULL
#define PTE_PRESENT 0x001ULL
#if CONFIG_MILESTONE < 8
#define PTE_WRITE 0x002ULL
#define PTE_USER 0x000ULL
#endif
#define PTE_HUGE 0x080ULL
#define PTE_ADDRESS 0x000ffffffffff000ULL

struct __attribute__((packed)) mb2_tag {
    uint32_t type;
    uint32_t size;
};

struct __attribute__((packed)) mb2_mmap_tag {
    uint32_t type;
    uint32_t size;
    uint32_t entry_size;
    uint32_t entry_version;
};

struct __attribute__((packed)) mb2_mmap_entry {
    uint64_t address;
    uint64_t length;
    uint32_t type;
    uint32_t reserved;
};

static uint64_t frame_bitmap[BITMAP_WORDS];
static size_t free_frames;
static uintptr_t heap_next;
static uintptr_t heap_end;
#if CONFIG_MILESTONE >= 8
static uint64_t *active_root = (uint64_t *)(void *)bootstrap_pml4;
#endif

static bool frame_is_used(size_t frame)
{
    const uint64_t mask = 1ULL << (frame % 64U);
    return (frame_bitmap[frame / 64U] & mask) != 0U;
}

static void bytes_zero(void *destination, size_t length)
{
    uint8_t *bytes = destination;
    for (size_t i = 0; i < length; ++i) {
        bytes[i] = 0;
    }
}

static void mark_frame(size_t frame, bool used)
{
    const uint64_t mask = 1ULL << (frame % 64U);
    uint64_t *const word = &frame_bitmap[frame / 64U];
    const bool was_used = (*word & mask) != 0U;
    if (used) {
        *word |= mask;
        if (!was_used && free_frames != 0U) {
            --free_frames;
        }
    } else {
        *word &= ~mask;
        if (was_used) {
            ++free_frames;
        }
    }
}

static void make_available(uint64_t start, uint64_t end)
{
    start = (start + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
    end &= ~(PAGE_SIZE - 1U);
    if (end > MAX_PHYSICAL_MEMORY) {
        end = MAX_PHYSICAL_MEMORY;
    }
    for (uint64_t address = start; address < end; address += PAGE_SIZE) {
        mark_frame((size_t)(address / PAGE_SIZE), false);
    }
}

static void reserve_range(uint64_t start, uint64_t end)
{
    start &= ~(PAGE_SIZE - 1U);
    end = (end + PAGE_SIZE - 1U) & ~(PAGE_SIZE - 1U);
    if (end > MAX_PHYSICAL_MEMORY) {
        end = MAX_PHYSICAL_MEMORY;
    }
    for (uint64_t address = start; address < end; address += PAGE_SIZE) {
        mark_frame((size_t)(address / PAGE_SIZE), true);
    }
}

uint64_t frame_alloc(void)
{
    for (size_t word = 0; word < BITMAP_WORDS; ++word) {
        if (frame_bitmap[word] == UINT64_MAX) {
            continue;
        }
        for (size_t bit = 0; bit < 64U; ++bit) {
            const uint64_t mask = 1ULL << bit;
            if ((frame_bitmap[word] & mask) == 0U) {
                frame_bitmap[word] |= mask;
                --free_frames;
                const uint64_t address = (word * 64U + bit) * PAGE_SIZE;
                bytes_zero((void *)(uintptr_t)address, PAGE_SIZE);
                return address;
            }
        }
    }
    return 0;
}

void frame_free(uint64_t physical)
{
    ASSERT((physical & (PAGE_SIZE - 1U)) == 0U);
    ASSERT(physical < MAX_PHYSICAL_MEMORY);
    ASSERT(physical != 0U);
    const size_t frame = (size_t)(physical / PAGE_SIZE);
    if (!frame_is_used(frame)) {
#if EXPECT_FAULT == 4
        serial_write("EXPECTED-FAULT: allocator PASS\n");
        qemu_exit(0x13);
#else
        PANIC("frame_free on an already-free frame");
#endif
    }
    mark_frame(frame, false);
}

static uint64_t *next_table(uint64_t *table, size_t index, uint64_t flags)
{
    if ((table[index] & PTE_PRESENT) == 0U) {
        const uint64_t frame = frame_alloc();
        if (frame == 0U) {
            return NULL;
        }
        table[index] = frame | PTE_PRESENT | PTE_WRITE | (flags & PTE_USER);
    }
    if ((table[index] & PTE_HUGE) != 0U) {
        return NULL;
    }
#if CONFIG_MILESTONE >= 8
    table[index] |= flags & PTE_USER;
#endif
    return (uint64_t *)(uintptr_t)(table[index] & PTE_ADDRESS);
}

static bool map_in_root(uint64_t *pml4, uint64_t virtual_address,
                        uint64_t physical, uint64_t flags)
{
    uint64_t *const pdpt = next_table(
        pml4, (size_t)((virtual_address >> 39U) & 0x1ffU), flags);
    if (pdpt == NULL) {
        return false;
    }
    uint64_t *const pd = next_table(
        pdpt, (size_t)((virtual_address >> 30U) & 0x1ffU), flags);
    if (pd == NULL) {
        return false;
    }
    uint64_t *const pt = next_table(
        pd, (size_t)((virtual_address >> 21U) & 0x1ffU), flags);
    if (pt == NULL) {
        return false;
    }
    const size_t index = (size_t)((virtual_address >> 12U) & 0x1ffU);
    if ((pt[index] & PTE_PRESENT) != 0U) {
        return false;
    }
    pt[index] = (physical & PTE_ADDRESS) | (flags & 0xfffU) | PTE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virtual_address) : "memory");
    return true;
}

bool map_page(uint64_t virtual_address, uint64_t physical, uint64_t flags)
{
    return map_in_root((uint64_t *)(void *)bootstrap_pml4,
                       virtual_address, physical, flags);
}

#if CONFIG_MILESTONE >= 8
uint64_t *address_space_create(void)
{
    const uint64_t root_frame = frame_alloc();
    const uint64_t pdpt_frame = frame_alloc();
    if (root_frame == 0U || pdpt_frame == 0U) {
        return NULL;
    }
    uint64_t *const root = (uint64_t *)(uintptr_t)root_frame;
    uint64_t *const pdpt = (uint64_t *)(uintptr_t)pdpt_frame;
    const uint64_t *const kernel_root = (const uint64_t *)(const void *)bootstrap_pml4;
    const uint64_t *const kernel_pdpt =
        (const uint64_t *)(uintptr_t)(kernel_root[0] & PTE_ADDRESS);
    for (size_t i = 0; i < 512U; ++i) {
        root[i] = kernel_root[i];
        pdpt[i] = kernel_pdpt[i];
    }
    root[0] = pdpt_frame | PTE_PRESENT | PTE_WRITE;
    return root;
}

bool map_page_in(uint64_t *root, uint64_t virtual_address, uint64_t physical,
                 uint64_t flags)
{
    if (root == NULL) {
        return false;
    }
    return map_in_root(root, virtual_address, physical, flags);
}

void address_space_activate(uint64_t *root)
{
    ASSERT(root != NULL);
    active_root = root;
    __asm__ volatile ("mov %0, %%cr3" : : "r"(root) : "memory");
}

static bool user_translate(uint64_t address, bool write, uint8_t **kernel_address,
                           size_t *contiguous)
{
    uint64_t *table = active_root;
    const unsigned shifts[4] = {39U, 30U, 21U, 12U};
    for (size_t level = 0; level < ARRAY_LEN(shifts); ++level) {
        const uint64_t entry = table[(address >> shifts[level]) & 0x1ffU];
        if ((entry & (PTE_PRESENT | PTE_USER)) != (PTE_PRESENT | PTE_USER)
            || (write && (entry & PTE_WRITE) == 0U)
            || (entry & PTE_HUGE) != 0U) {
            return false;
        }
        if (level == 3U) {
            const size_t offset = (size_t)(address & (PAGE_SIZE - 1U));
            *kernel_address = (uint8_t *)(uintptr_t)
                ((entry & PTE_ADDRESS) + offset);
            *contiguous = (size_t)PAGE_SIZE - offset;
            return true;
        }
        table = (uint64_t *)(uintptr_t)(entry & PTE_ADDRESS);
    }
    return false;
}

static bool user_copy(void *kernel_buffer, uint64_t user_address, size_t length,
                      bool to_user)
{
    uint8_t *kernel = kernel_buffer;
    while (length != 0U) {
        uint8_t *mapped;
        size_t available;
        if (!user_translate(user_address, to_user, &mapped, &available)) {
            return false;
        }
        const size_t count = length < available ? length : available;
        for (size_t i = 0; i < count; ++i) {
            if (to_user) {
                mapped[i] = kernel[i];
            } else {
                kernel[i] = mapped[i];
            }
        }
        kernel += count;
        user_address += count;
        length -= count;
    }
    return true;
}

bool copy_from_user(void *destination, uint64_t user_source, size_t length)
{
    if (destination == NULL || length > 4096U) {
        return false;
    }
    return user_copy(destination, user_source, length, false);
}

bool copy_to_user(uint64_t user_destination, const void *source, size_t length)
{
    if (source == NULL || length > 4096U) {
        return false;
    }
    return user_copy((void *)source, user_destination, length, true);
}
#endif

void *kalloc(size_t bytes)
{
    if (bytes == 0U) {
        return NULL;
    }
    const uintptr_t aligned = (heap_next + 15U) & ~(uintptr_t)15U;
    if (aligned > heap_end || bytes > heap_end - aligned) {
        return NULL;
    }
    heap_next = aligned + bytes;
    return (void *)aligned;
}

void memory_init(uint64_t multiboot_info)
{
    for (size_t i = 0; i < BITMAP_WORDS; ++i) {
        frame_bitmap[i] = UINT64_MAX;
    }
    free_frames = 0;

    const uint32_t total_size = *(const uint32_t *)(uintptr_t)multiboot_info;
    const uint8_t *cursor = (const uint8_t *)(uintptr_t)(multiboot_info + 8U);
    const uint8_t *const limit =
        (const uint8_t *)(uintptr_t)(multiboot_info + total_size);
    unsigned regions = 0;
    while (cursor + sizeof(struct mb2_tag) <= limit) {
        const struct mb2_tag *const tag = (const struct mb2_tag *)(const void *)cursor;
        if (tag->type == 0U) {
            break;
        }
        ASSERT(tag->size >= sizeof(*tag));
        if (tag->type == 6U) {
            const struct mb2_mmap_tag *const map =
                (const struct mb2_mmap_tag *)(const void *)tag;
            ASSERT(map->entry_size >= sizeof(struct mb2_mmap_entry));
            const uint8_t *entry_cursor = cursor + sizeof(*map);
            const uint8_t *const map_limit = cursor + tag->size;
            while (entry_cursor + map->entry_size <= map_limit) {
                const struct mb2_mmap_entry *const entry =
                    (const struct mb2_mmap_entry *)(const void *)entry_cursor;
                if (entry->type == 1U) {
                    const uint64_t end = entry->length > UINT64_MAX - entry->address
                        ? UINT64_MAX : entry->address + entry->length;
                    make_available(entry->address, end);
                    ++regions;
                }
                entry_cursor += map->entry_size;
            }
        }
        cursor += (tag->size + 7U) & ~7U;
    }
    ASSERT(regions != 0U);

    reserve_range(0, (uint64_t)(uintptr_t)__kernel_end);
    reserve_range(multiboot_info, multiboot_info + total_size);
    ASSERT(free_frames >= 16U);
    serial_write("MEMORY: usable regions=");
    serial_dec(regions);
    serial_write(" bounded-free-frames=");
    serial_dec(free_frames);
    serial_write("\n");

    const uint64_t probe = frame_alloc();
    ASSERT(probe != 0U);
    frame_free(probe);
    serial_write("SELFTEST: frame allocator PASS\n");
#if EXPECT_FAULT == 4
    serial_write("INJECT: allocator double-free\n");
    frame_free(probe);
    PANIC("allocator misuse was not detected");
#endif

    const uint64_t mapped_frame = frame_alloc();
    ASSERT(mapped_frame != 0U);
    ASSERT(map_page(0x40000000ULL, mapped_frame, PTE_WRITE));
    *(volatile uint64_t *)(uintptr_t)0x40000000ULL = 0x544f594f53504147ULL;
    ASSERT(*(volatile uint64_t *)(uintptr_t)0x40000000ULL == 0x544f594f53504147ULL);
    serial_write("SELFTEST: mapper PASS\n");

    for (uint64_t page = 0; page < HEAP_PAGES; ++page) {
        const uint64_t frame = frame_alloc();
        ASSERT(frame != 0U);
        ASSERT(map_page(HEAP_BASE + page * PAGE_SIZE, frame, PTE_WRITE));
    }
    heap_next = HEAP_BASE;
    heap_end = HEAP_BASE + HEAP_PAGES * PAGE_SIZE;
    uint64_t *const allocation = kalloc(sizeof(*allocation));
    ASSERT(allocation != NULL);
    *allocation = 0x48454150ULL;
    ASSERT(*allocation == 0x48454150ULL);
    serial_write("SELFTEST: kernel heap PASS\n");
}

#endif
