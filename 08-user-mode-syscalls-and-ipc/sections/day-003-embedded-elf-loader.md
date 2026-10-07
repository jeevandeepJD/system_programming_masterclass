# Day 3 — Embedded ELF Validation, `PT_LOAD`, Entry, and Stack

**Target time:** approximately 3 hours

> An executable loader is a parser at a security boundary. “The compiler made
> this file” does not remove the need to validate lengths, arithmetic, and
> destination policy.

## 1. Establish where the bytes come from

Week 8 does not read a filesystem. The Makefile first links
`build/week8/user.elf` from `user_program.o` with `user.ld`. It then assembles
`src/user_image.S` with an absolute `USER_IMAGE` path so the ELF bytes become
part of a kernel object. Linker symbols `user_elf_start` and `user_elf_end`
bound that embedded byte array.

This arrangement makes the lesson deterministic and bootstraps before Week 9
VFS work. It is still a real ELF64 image with an ELF header and program
headers. The loader parses those bytes rather than relying on kernel section
names.

Artifact identity matters. `readelf -h -l build/week8/user.elf` describes the
user image; `build/week8/toyos.elf` is the kernel containing it. Do not use
one symbol table to explain the other.

## 2. Validate before pointer arithmetic

`load_elf` first computes `image_size` from the linker bounds and requires at
least one complete `elf_header`. It validates:

- magic `0x7f 'E' 'L' 'F'`;
- class 2, meaning ELF64;
- data 1, meaning little-endian;
- machine 62, meaning x86-64;
- exact program-header entry size;
- a nonzero program-header count;
- the complete program-header table lies within the embedded image.

The helper `range_inside(offset, length, total)` uses subtraction after
checking `offset <= total`, avoiding the classic `offset + length` overflow
test. That pattern is worth carrying into every binary parser.

There are limits. Multiplication of `phnum * sizeof(program_header)` occurs
after casts to `size_t` without an explicit multiplication-overflow check.
`e_type`, ELF version, OS ABI, alignment, and header-size fields are not fully
validated. The image is trusted build output, but the code should not be
described as a hardened general ELF loader.

## 3. Treat program headers as the load contract

The loader iterates `header->phnum` entries and skips anything not
`PT_LOAD`. For each loadable segment it checks:

- `memory_size >= file_size`, so initialized bytes fit in memory extent;
- file offset and file size remain within the image;
- virtual address is at least `0x40000000`;
- virtual-address addition does not wrap;
- the end stays below the four-page stack region.

This separates file extent from memory extent. ELF permits `p_memsz` to
exceed `p_filesz`; the difference usually represents zero-initialized data.
`frame_alloc` zeroes every new frame, so bytes not copied from the file begin
as zero. That implicit dependency should be documented whenever allocator
behavior changes.

The loader rounds the segment start down and end up to page boundaries,
allocates one frame per page, and maps every leaf `PTE_USER | PTE_WRITE`.
It then walks the newly built page tables manually for each file byte and
copies into the corresponding physical frame through the identity map.

## 4. Identify policy hidden in compact code

The mapper ignores ELF `p_flags`: text and read-only data become writable,
and NX is not enforced. This violates W^X expectations of hardened systems.
It is an explicit project limit, not an x86 limitation.

Overlapping `PT_LOAD` page ranges would cause the second `map_page_in` to
reject an already-present leaf. There is no rollback, so prior frames and
tables leak when loading fails. The loader also does not track which frames
belong to which segment.

The byte-at-a-time copy is simple but inefficient. A production loader would
copy runs, carefully handle partial first and last pages, zero BSS, apply
permissions after relocation/loading, and unwind allocations on failure.

Most importantly, the code stores `header->entry` without proving it lies in
a loaded segment or carries intended execute permission. In this canonical
image it does. As a parser claim, however, segment-range validation does not
automatically validate the entry point.

## 5. Build the initial user stack

After loading segments, `process_init` allocates four frames and maps:

```text
0x4001c000 through 0x4001ffff
```

with `USER | WRITE`, then records `USER_STACK_TOP` as `0x40020000`. The stack
grows downward. It starts empty: no `argc`, `argv`, `envp`, auxiliary vector,
return address, or randomized gap is installed. The assembly user program is
written to need none of them.

There is no explicit guard-page test. The next page below the mapped range is
expected to be absent unless an ELF segment occupied it, which the segment
upper bound prevents. There is no automatic growth; an access below the four
pages faults.

The kernel stack is separate. It is allocated from the supervisor heap and
is never mapped user-accessible. The TSS points to its aligned top for
privilege entry. User RSP and kernel RSP are different resources with
different owners and trust levels.

## 6. Inspect ELF evidence precisely

Use `readelf` to answer concrete questions:

- What is the ELF entry address?
- How many program headers exist?
- Which headers are `LOAD`?
- What are each header's file size, memory size, flags, and alignment?
- Do virtual ranges stay below the stack reservation?

Use `nm -n build/week8/user.elf` or `objdump -d` to associate `_user_start`
with its instructions. Use the kernel ELF to locate `load_elf`,
`process_init`, and embedded-image symbols.

The wrapper checks runtime markers too, but a successful boot exercises only
the canonical ELF. It does not fuzz malformed headers. Static inspection of
validation branches supports the control logic; it is not equivalent to
executing every rejection.

## 7. Compare executable loading models

xv6 `exec` reads an executable through its filesystem, creates/replaces a
process page table, loads segments, builds a user stack containing arguments,
and commits the new image after successful preparation. Its executable
format and architecture details should be checked against the current tree.

Linux `execve` is a large policy boundary involving binary-format handlers,
credentials, VMAs, demand paging, argument/environment limits, auxiliary
vectors, interpreter loading, ASLR, permission checks, and cleanup. ELF
program headers are still central, but the lifecycle and security model are
far richer.

ToyOS isolates one idea: validated file ranges become owned physical pages
and user translations before a controlled entry. Preserve that causal model
while being explicit about absent commit/rollback and protection policy.

## Practical challenge

Use `challenges/day-003-elf-loader.sh` from the repository root.

**Purpose:** correlate loader checks with the exact embedded user ELF and
normal execution.

**Predict:** identify the entry, `PT_LOAD` ranges, four-page stack interval,
normal host status 33, and process-loaded marker.

**Evidence:** the wrapper builds Week 8, runs `readelf` on the exact user
artifact, displays loader contracts, checks QEMU, and prints loader symbols.

## Mastery prompts

For each accepted segment, record file extent, memory extent, rounded page
extent, allocated frame owner, PTE permissions, and failure cleanup. Then
list three malformed ELF cases rejected by current code and three that need
stronger validation.
