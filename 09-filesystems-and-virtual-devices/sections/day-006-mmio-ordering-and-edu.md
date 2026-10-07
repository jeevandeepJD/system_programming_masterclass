# Day 6 — MMIO Register Access, Ordering, and EDU Boundaries

**Target time:** approximately 3 hours

> `volatile` constrains compiler treatment of an access. It is not a complete
> device-ordering API, a lock, an interrupt protocol, or a DMA ownership model.

## 1. Exercise the one implemented operation

Run `challenges/day-006-mmio-edu.sh`. Predict that the kernel reads EDU ID
`0x010000ed`, writes `0x12345678` to liveness offset `0x04`, and reads back
the bitwise inverse. Predict also that the device rejection profile calls
`edu_checked_write(3, 0)`, rejects the unaligned and unauthorized offset before
MMIO, emits its marker, and exits with host status 49.

The rejection profile first completes normal EDU initialization. It therefore
tests the helper's software validation after discovery and liveness have
succeeded. It does not provoke a hardware bus fault or ask EDU to reject an
operation.

## 2. Understand the C expression and the bus operation

`edu_mmio` has type `volatile uint32_t *`. Indexing by
`offset / sizeof(uint32_t)` forms a 32-bit access at the selected register.
The `volatile` qualification tells the compiler that accesses are observable:
it should emit them rather than remove, merge, or satisfy them solely from a
register under normal compiler rules.

That property is necessary for this direct style but insufficient as a
general driver interface:

- It does not make a multi-step protocol atomic.
- It does not serialize two CPUs.
- It does not define device-endian conversion.
- It does not select page-table memory attributes.
- It does not necessarily provide every ordering relation needed around
  ordinary memory and MMIO on every architecture.
- It does not flush posted writes merely because C execution reached the next
  statement.

Production kernels wrap register access in architecture-aware helpers such as
read/write APIs. Those helpers encode width, endianness, barriers, and
platform quirks more reliably than scattered volatile pointer expressions.

## 3. Validate registers before touching them

`edu_checked_write` accepts a write only when:

1. the base pointer has been installed;
2. offset is exactly the liveness register `0x04`;
3. offset is four-byte aligned;
4. offset lies within the mapped page.

The exact-offset condition is strongest here; it prevents the helper from
writing the read-only ID register or any unimplemented EDU register. The
alignment and range checks document general MMIO hazards even though equality
already implies them for the one allowed offset.

Order the checks so no pointer arithmetic or device access occurs first.
Invalid offsets are untrusted driver inputs even when only kernel code calls
the helper today. If later exposed indirectly to users, validation remains in
kernel policy.

The helper returns false rather than panicking. `edu_init` chooses to assert
success for its known liveness write. This separates a reusable checked
operation from boot-time policy.

## 4. Reason about ordering by architecture

x86 has a relatively strong memory model, and port I/O instructions plus
uncacheable MMIO have architecture-defined ordering properties. Compiler
barriers and proper MMIO mapping attributes still matter. A read from a
device can also be used according to the device/platform contract to drain a
posted write, but an arbitrary read is not universally a safe flush.

AArch64 is more weakly ordered. MMIO should be mapped as device memory with
the correct shareability and access attributes. Kernel accessors may include
ordering, while explicit `dmb` or `dsb` is required at protocol-defined
boundaries. The distinction between ordering accesses and waiting for effects
to complete matters.

RISC-V likewise requires attention to I/O versus memory ordering and may use
`fence` with appropriate predecessor/successor sets. Platform memory
attributes and coherent interconnect behavior are part of the contract.

Do not cargo-cult a barrier after every access. Start from the protocol:
which prior ordinary-memory writes must be visible before a doorbell? Which
status read must occur after a command? Which CPU may race? Then choose the
kernel accessor and barrier that creates those relations on the target ISA.

ToyOS performs a simple write followed by a read of the same liveness
register. The QEMU EDU model returns the inverse, providing a deterministic
functional check. The project does not present this as a portable ordering
library.

## 5. The EDU operation and evidence

The sequence is:

```text
mapped base established
ID[0x00] read as 0x010000ed
checked write of 0x12345678 to LIVENESS[0x04]
LIVENESS[0x04] read as ~0x12345678
serial pass marker emitted
```

The readback demonstrates that the selected mapping reaches a device
implementing the expected EDU liveness semantics in this QEMU run. It helps
catch a wrong BAR, wrong virtual mapping, wrong offset, or absent device.
It does not prove behavior on physical hardware, stress ordering, or validate
other EDU registers.

When the value is wrong, record raw values instead of immediately adding
delays. Verify device presence, raw BAR, physical mask, PTE, memory type,
virtual address, access width, alignment, register offset, and QEMU version.
For a fault, also record saved RIP, fault address, and page-fault error bits.

## 6. IRQ is an explicit future boundary

EDU can generate interrupts, but Week 9 does not configure or claim them.
Adding IRQ operation requires at least:

- a known routing mechanism and interrupt controller path;
- interrupt enable/acknowledge semantics from EDU's specification;
- a handler registered before enabling the source;
- shared state whose lifetime extends until completion;
- synchronization between process and interrupt context;
- timeout and spurious/unexpected interrupt behavior;
- masking and teardown order that prevent use-after-free.

This minimal kernel uses a legacy PIC for earlier timer work. PCI INTx routing
under the selected virtual machine is not established here. MSI/MSI-X setup
is also absent. Polling one register is not evidence of interrupt delivery.

## 7. DMA is a separate future boundary

EDU also supports DMA, but Week 9 does not enable bus mastering or submit a
DMA command. A correct DMA path must distinguish virtual from physical/device
addresses, ensure the device can address the buffer, pin ownership until
completion, validate size and alignment, handle cache coherency, and prevent
the device from accessing unrelated memory. An IOMMU can improve isolation
but requires its own mapping and lifetime model.

ToyOS's frame allocator and identity-access assumptions do not provide a
complete DMA API. Passing a C pointer to a device register would be an unsafe
fiction. The right next claim remains: “IRQ and DMA are not implemented.”

## Practical challenge

**Purpose:** connect a checked C register access to device-defined behavior
while separating compiler, CPU, mapping, and device contracts.

**Prediction:** liveness returns the inverse; offset 3 is rejected in software
and the expected profile reaches host status 49.

**Evidence:** helper and mapping source, exact instructions, normal checked
operation, and checked invalid-register profile.

**Limits:** no portable MMIO accessor layer, explicit cache-attribute API,
concurrency, physical-device validation, IRQ completion, or DMA exists.
