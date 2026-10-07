#include "toyos.h"

#if CONFIG_MILESTONE >= 9

#define PCI_CONFIG_ADDRESS 0xcf8U
#define PCI_CONFIG_DATA 0xcfcU
#define EDU_VENDOR_DEVICE 0x11e81234U
#define EDU_MMIO_VIRTUAL 0x60000000ULL
#define EDU_ID 0x00U
#define EDU_LIVENESS 0x04U
#define EDU_EXPECTED_ID 0x010000edU

static volatile uint32_t *edu_mmio;

static inline void outl(uint16_t port, uint32_t value)
{
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port)
{
    uint32_t value;
    __asm__ volatile ("inl %1, %0" : "=a"(value) : "Nd"(port));
    return value;
}

static uint32_t pci_read(unsigned device, unsigned offset)
{
    const uint32_t address = 0x80000000U
        | ((uint32_t)device << 11U) | (uint32_t)(offset & 0xfcU);
    outl(PCI_CONFIG_ADDRESS, address);
    return inl(PCI_CONFIG_DATA);
}

bool edu_checked_write(uint32_t offset, uint32_t value)
{
    if (edu_mmio == NULL || offset != EDU_LIVENESS
        || (offset & 3U) != 0U || offset >= PAGE_SIZE) {
        return false;
    }
    edu_mmio[offset / sizeof(uint32_t)] = value;
    return true;
}

void edu_init(void)
{
    unsigned device = 32U;
    for (unsigned candidate = 0; candidate < 32U; ++candidate) {
        if (pci_read(candidate, 0U) == EDU_VENDOR_DEVICE) {
            device = candidate;
            break;
        }
    }
    ASSERT(device < 32U);
    const uint32_t bar = pci_read(device, 0x10U);
    ASSERT((bar & 1U) == 0U);
    const uint64_t physical = (uint64_t)(bar & ~0xfU);
    ASSERT(physical != 0U);
    ASSERT(map_page(EDU_MMIO_VIRTUAL, physical, PTE_WRITE));
    edu_mmio = (volatile uint32_t *)(uintptr_t)EDU_MMIO_VIRTUAL;
    ASSERT(edu_mmio[EDU_ID / sizeof(uint32_t)] == EDU_EXPECTED_ID);
    ASSERT(edu_checked_write(EDU_LIVENESS, 0x12345678U));
    ASSERT(edu_mmio[EDU_LIVENESS / sizeof(uint32_t)] == ~0x12345678U);
    serial_write("EDU: PCI discovery/BAR/MMIO liveness PASS\n");
#if EXPECT_FAULT == 9
    ASSERT(!edu_checked_write(3U, 0U));
    serial_write("EXPECTED-FAULT: device PASS\n");
    qemu_exit(0x18);
#endif
}

#endif
