# Day 5 — PCI Configuration Enumeration, BARs, and QEMU EDU

**Target time:** approximately 3 hours

> Discovery reads a standardized configuration interface. A matched ID tells
> the kernel what device may be present; it does not prove a usable register
> mapping or a completed operation.

## 1. Predict the discovery transaction

Run `challenges/day-005-pci-edu.sh`. Before execution, predict that ToyOS scans
device numbers 0 through 31 on bus 0, function 0; finds the combined
vendor/device value `0x11e81234`; reads BAR0 at offset `0x10`; requires a
memory BAR and nonzero base; maps one page; then checks EDU's ID register.

The wrapper invokes QEMU with `-device edu` through the project's canonical
test target. Omitting that option is not a valid negative test under the
current code because `edu_init` asserts that discovery succeeds and would
panic rather than emit a dedicated expected marker.

## 2. Configuration mechanism #1 on x86

ToyOS uses legacy PCI configuration mechanism #1:

```text
CONFIG_ADDRESS port 0xcf8
CONFIG_DATA    port 0xcfc
```

It forms a 32-bit address with enable bit 31, device in bits 15:11, and the
dword-aligned register offset in bits 7:2. Bus and function fields remain
zero. It writes the address with `outl`, then reads the data with `inl`.

Conceptually, the full address is:

```text
enable | bus<<16 | device<<11 | function<<8 | (offset & 0xfc)
```

Because ToyOS scans only bus 0/function 0, it cannot discover devices behind
bridges, on other buses, or in nonzero functions. It also does not inspect
header type to determine multifunction behavior. This is sufficient for the
QEMU machine arrangement selected by the test, not general PCI enumeration.

The inline assembly is x86-specific. I/O ports and `inl/outl` are not portable
C and do not exist as the PCI access mechanism on many AArch64 or RISC-V
platforms. Those systems commonly expose an ECAM or host-controller MMIO
window described by ACPI or device tree. The generic PCI concepts—BDF,
configuration space, IDs, BARs—remain; the access mechanism and discovery
source differ.

## 3. Interpret vendor and device IDs precisely

Reading offset zero returns vendor ID in the low 16 bits and device ID in the
high 16 bits. On little-endian x86, QEMU EDU vendor `0x1234` and device
`0x11e8` appear in a 32-bit read as `0x11e81234`.

ToyOS compares that complete value. It does not inspect class code, revision,
subsystem IDs, capabilities, or command register. A match identifies the
device model expected by this lesson, but robust binding normally combines
tables, class information, firmware policy, and driver-specific checks.

No claim should be made that the scan “configures PCI.” It reads enough
configuration state to locate one device and one BAR. It does not assign bus
numbers or resources, enable bus mastering, negotiate MSI/MSI-X, or preserve
firmware topology.

## 4. Decode BAR0 without overgeneralizing

At configuration offset `0x10`, BAR0 describes an I/O or memory resource. Bit
0 distinguishes I/O space from memory space. ToyOS asserts bit 0 is clear and
masks the low four flag bits:

```text
physical = bar & ~0xf
```

It also requires a nonzero result. This is adequate for the selected 32-bit
memory BAR assigned by QEMU. General BAR handling must inspect memory type,
possibly combine adjacent low/high dwords for a 64-bit BAR, account for
prefetchability, determine resource size, and validate assignment. Drivers
must not casually write all ones to a live BAR to size it after the platform
has configured resources without appropriate coordination.

ToyOS maps exactly one 4 KiB page at virtual address `0x60000000` with
`PTE_WRITE`. It assumes BAR0 is page aligned and that the used registers fit
in that page. The page tables currently do not express a dedicated
uncacheable/device memory type. On x86, platform MTRR/PAT state and the QEMU
environment make this selected test work, but a production mapping API must
choose suitable memory attributes explicitly.

## 5. Separate four validation stages

A useful driver bring-up report records:

1. **Discovery:** which BDF produced which vendor/device dword?
2. **Resource decoding:** what raw BAR was read, what flags were removed, and
   what physical range is claimed?
3. **Mapping:** what virtual range and page-table attributes expose it?
4. **Device identity:** what value did the mapped ID register return?

Failure at one stage should not be debugged as another. If no ID matches,
inspect QEMU arguments and config transactions. If BAR bit 0 indicates I/O,
do not debug page tables. If the mapping faults, inspect the PTE and physical
base before questioning EDU semantics. If the ID is wrong but reads are
stable, verify register width, endianness, offset, and device revision.

Artifact identity includes more than the ELF. The virtual hardware command
line is part of the experiment. A report should record QEMU version and the
presence of `-device edu`.

## 6. Architecture and policy boundaries

The PCI specification assigns configuration fields and BAR meaning. x86
defines I/O instructions and memory-order behavior. QEMU EDU defines its
register protocol. ToyOS policy decides to scan only 32 slots, bind the first
exact ID, choose virtual address `0x60000000`, map one page writable, and panic
on mismatch.

On AArch64, page tables must map device registers with a device memory type;
ordinary cacheable memory is wrong. On RISC-V, platform descriptions and
physical memory attributes matter, and ordering may require `fence`. These
differences are relevant when porting `edu.c`, but they do not change VFS
descriptor policy.

## 7. Define what discovery does not establish

Successful enumeration does not establish interrupt routing, DMA safety,
concurrent access, reset handling, hotplug, power management, error recovery,
or performance. It also does not establish that every register offset is
valid. Day 6 examines checked access and ordering.

The narrow Week 9 claim is still valuable: the canonical kernel issues real
x86 PCI configuration transactions against QEMU's EDU model, decodes its
assigned BAR, maps that physical resource, and validates an EDU-defined ID.
That is stronger than a mock structure and weaker than a complete driver.

## Practical challenge

**Purpose:** reconstruct PCI config addresses and validate the chain from ID
match through BAR mapping.

**Prediction:** one bus-0/function-0 slot matches `1234:11e8`, BAR0 is a
nonzero memory BAR, and the normal profile reaches the EDU pass marker.

**Evidence:** exact source, ELF symbols and disassembly, QEMU version and
command contract, plus checked serial/status output.

**Limits:** no bridge walk, multifunction scan, ECAM, resource sizing,
64-bit BAR handling, interrupt setup, DMA enablement, or hotplug is tested.
