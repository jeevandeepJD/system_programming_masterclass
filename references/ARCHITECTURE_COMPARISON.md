# Architecture Comparison for Systems Programmers

## Scope

The primary comparison set is:

- x86-64
- AArch64
- RISC-V 64-bit application and privileged architectures

POWER, s390, LoongArch, and 32-bit variants are mentioned only when they expose
an important assumption. “RISC versus CISC” is not used as a performance
ranking; modern implementations overlap heavily while their architectural
contracts remain different.

## Architectural state and instruction style

### x86-64

- Variable-length instructions, commonly 1–15 bytes.
- General integer operations may use register or memory operands.
- Sixteen architectural general-purpose registers in the base 64-bit ISA.
- `RIP` and `RFLAGS` are architecturally visible.
- Many instructions update condition flags consumed by later branches.
- Current high-performance implementations often decode instructions into
  internal micro-operations; those are microarchitectural, not ISA-visible.

### AArch64

- Fixed 32-bit instruction width in A64 state.
- Thirty-one general-purpose integer registers plus a context-dependent zero
  register/stack-pointer encoding.
- Load/store architecture: arithmetic normally uses registers.
- `PC` is not a general-purpose register in A64.
- Integer condition flags live in `NZCV`; many instructions can choose whether
  to update them.

### RISC-V

- Base instructions are 32 bits; optional compressed instructions are 16 bits.
- Thirty-two integer registers; `x0` always reads zero and discards writes.
- Load/store architecture.
- No general integer condition-code register; branches compare registers
  directly.
- ISA behavior depends on selected extensions, such as `C`, `A`, `V`, `H`,
  and privileged-specification version.

## Calling conventions

- x86-64 Linux normally uses the System V AMD64 ABI: first integer arguments
  in `RDI, RSI, RDX, RCX, R8, R9`; return in `RAX`; stack alignment and red-zone
  rules apply.
- AArch64 uses AAPCS64: first integer arguments in `x0–x7`; return in `x0`;
  `x30` is the link register; the stack pointer remains 16-byte aligned.
- RISC-V psABI uses `a0–a7` for arguments, `a0–a1` for returns, `ra` for the
  return address, and `sp` for the stack.
- Register ownership, stack layout, unwind metadata, and variadic rules belong
  to the ABI, not merely to the ISA.

## System calls and privilege

### x86-64

- Linux user programs usually enter with `syscall`.
- Privilege uses CPL/rings; Linux primarily uses ring 3 and ring 0.
- Exceptions/interrupts vector through the IDT.
- `sysret` or `iretq` may participate in returns depending on the path and
  required state restoration.

### AArch64

- Linux user programs use `svc`.
- Exception levels provide the privilege structure: EL0 userspace, EL1
  kernel, EL2 hypervisor, EL3 secure monitor/firmware.
- Exception entry branches through a vector table selected by `VBAR_ELx`,
  with distinct slots based on source level, stack choice, and exception type.
- `eret` returns from an exception.

### RISC-V

- `ecall` requests an environment call.
- Common privilege modes are U, S, and M; hypervisor support adds VS/VU
  virtualization state.
- Trap targets come from `stvec`/`mtvec`; cause and return state are recorded
  in CSRs such as `scause`, `sepc`, and `stval`.
- `sret`/`mret` return from traps.

## Interrupt controllers

- x86 systems commonly use local APIC/x2APIC plus I/O APIC and MSI/MSI-X.
- AArch64 server/mobile systems commonly use an ARM GIC generation.
- RISC-V systems may use PLIC/CLINT-style platforms or the newer AIA family.
- The CPU exception architecture and the external interrupt controller are
  related but separate mechanisms.

## Virtual memory and TLBs

- x86-64 commonly uses multi-level page tables with 4 KiB base pages and large
  pages; exact virtual-address width and level count are feature/configuration
  dependent.
- AArch64 translation depends on configured granule, address size, and level
  structure; 4 KiB, 16 KiB, and 64 KiB granules are architectural options.
- RISC-V defines schemes such as Sv39, Sv48, and Sv57; software selects a
  supported mode through `satp`.
- Page-table formats and fault-status reporting differ, but all three need
  translation caching, permission checks, invalidation, and coordination when
  mappings change.
- Linux hides many differences behind generic MM code while architecture code
  supplies PTE formats, TLB maintenance, fault entry, and context-switch hooks.

## Memory ordering and atomics

- x86-64 provides a relatively strong TSO-style model but still allows effects
  such as store buffering; compiler barriers and atomic-language semantics
  remain necessary.
- AArch64 is more weakly ordered and provides explicit acquire/release
  operations plus `dmb`, `dsb`, and `isb` barriers.
- RISC-V's base memory model is RVWMO; fences, acquire/release bits, AMOs, and
  LR/SC from the `A` extension provide ordering/atomic mechanisms.
- Portable C/C++ atomics express language-level ordering. The compiler maps
  that contract onto architecture instructions; `volatile` is not a substitute.
- Linux memory-barrier APIs deliberately describe required relationships and
  map them per architecture.

## Alignment and byte order

- Mainstream x86-64, AArch64 Linux, and RISC-V Linux deployments are normally
  little-endian, but code should not convert that ecosystem fact into a C or
  protocol guarantee.
- x86 tolerates many unaligned accesses, sometimes with performance or
  atomicity consequences.
- AArch64 and RISC-V support depends more visibly on access type, configuration,
  implementation, and platform handling; some accesses trap or are emulated.
- C alignment and object rules apply even when hardware happens to tolerate
  an unaligned instruction.

## Virtualization

- x86 virtualization uses Intel VMX or AMD SVM, with EPT/NPT for second-level
  translation and architecture-specific VM-control state.
- AArch64 virtualization is centered on EL2, stage-2 translation, and GIC
  virtualization support.
- RISC-V's hypervisor extension adds HS/VS/VU concepts and second-stage
  translation schemes.
- KVM exposes a common userspace API where possible, but vCPU state,
  interrupt delivery, page-table maintenance, exit reasons, and firmware
  contracts remain architecture-specific.

## IOMMU and device assignment

- x86 platforms use Intel VT-d or AMD IOMMU.
- AArch64 commonly uses an ARM SMMU.
- RISC-V has a standard IOMMU architecture with platform adoption still
  evolving.
- Generic Linux DMA/IOMMU/VFIO APIs hide some differences; page-table formats,
  invalidation, interrupt remapping, coherency, and firmware description differ.

## Linux source landmarks

- x86: `arch/x86/`
- AArch64: `arch/arm64/`
- RISC-V: `arch/riscv/`
- Generic mechanisms live in directories such as `kernel/`, `mm/`, `fs/`,
  `net/`, `drivers/`, and `virt/`, with architecture hooks providing the
  machine-specific implementation.

When studying a Linux path:

1. identify the generic subsystem entry;
2. identify the architecture hook;
3. find the saved architectural state and calling convention;
4. note the interrupt/exception/privilege context;
5. compare the same path in at least one other architecture.

## Primary references

- Intel 64 and IA-32 Software Developer's Manuals
- AMD64 Architecture Programmer's Manuals
- Arm Architecture Reference Manual for A-profile architecture
- Arm AAPCS64
- RISC-V unprivileged and privileged architecture specifications
- RISC-V ELF psABI
- System V AMD64 ABI
- Current Linux source and `Documentation/arch/`
