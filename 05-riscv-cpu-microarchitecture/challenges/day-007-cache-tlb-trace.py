#!/usr/bin/env python3
"""Safe cache/TLB/MMU observation model with deterministic self-tests.

This standard-library-only lab models a single user address space, a tiny
fully associative TLB, separate direct-mapped instruction/data caches, and
an OS-created page-table dictionary.  It does not inspect or alter host page
tables, mappings, cache state, or privileged registers.

Run from the repository root:

    python3 05-riscv-cpu-microarchitecture/challenges/day-007-cache-tlb-trace.py trace
    python3 05-riscv-cpu-microarchitecture/challenges/day-007-cache-tlb-trace.py selftest

The cycle costs and structures are teaching parameters, not measurements or
a cycle-accurate RISC-V implementation.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from typing import Iterable


PAGE_SIZE = 4096
LINE_SIZE = 16
CACHE_SETS = 4
TLB_HIT_CYCLES = 1
PAGE_WALK_CYCLES = 6
CACHE_HIT_CYCLES = 1
CACHE_FILL_CYCLES = 9
FAULT_CYCLES = 6

VALID_ACCESS = frozenset({"fetch", "load", "store"})


@dataclass(frozen=True)
class PTE:
    """Simplified OS-created leaf page-table entry."""

    ppn: int
    user: bool
    read: bool
    write: bool
    execute: bool

    def permits(self, access: str, user_mode: bool) -> bool:
        if user_mode and not self.user:
            return False
        return {
            "fetch": self.execute,
            "load": self.read,
            "store": self.write,
        }[access]


@dataclass(frozen=True)
class Translation:
    physical_address: int
    cycles: int
    tlb_hit: bool


@dataclass(frozen=True)
class CacheResult:
    cycles: int
    hit: bool
    index: int
    tag: int
    evicted_tag: int | None


@dataclass(frozen=True)
class RequestResult:
    access: str
    virtual_address: int
    physical_address: int | None
    cycles: int
    completed: bool
    tlb_hit: bool | None
    cache_hit: bool | None
    fault_kind: str | None
    events: tuple[str, ...]


class TranslationFault(RuntimeError):
    """Architectural translation or permission failure."""

    def __init__(self, kind: str, access: str, virtual_address: int) -> None:
        self.kind = kind
        self.access = access
        self.virtual_address = virtual_address
        super().__init__(f"{kind}: {access} VA 0x{virtual_address:08x}")


class TLB:
    """Small fully associative FIFO TLB keyed by (ASID, VPN)."""

    def __init__(self, capacity: int = 4) -> None:
        if capacity < 1:
            raise ValueError("TLB capacity must be positive")
        self.capacity = capacity
        self.entries: dict[tuple[int, int], PTE] = {}
        self.order: list[tuple[int, int]] = []
        self.hits = 0
        self.misses = 0

    def translate(
        self,
        virtual_address: int,
        access: str,
        page_table: dict[int, PTE],
        *,
        asid: int = 1,
        user_mode: bool = True,
        events: list[str] | None = None,
    ) -> Translation:
        if virtual_address < 0:
            raise ValueError("virtual address must be nonnegative")
        if access not in VALID_ACCESS:
            raise ValueError(f"unsupported access kind: {access}")

        log = events if events is not None else []
        vpn, offset = divmod(virtual_address, PAGE_SIZE)
        key = (asid, vpn)

        if key in self.entries:
            pte = self.entries[key]
            self.hits += 1
            cycles = TLB_HIT_CYCLES
            hit = True
            log.append(
                f"[microarchitecture] TLB HIT  asid={asid} vpn=0x{vpn:x}"
            )
        else:
            self.misses += 1
            cycles = PAGE_WALK_CYCLES
            hit = False
            log.append(
                f"[microarchitecture] TLB MISS asid={asid} vpn=0x{vpn:x}"
            )
            log.append("[microarchitecture] hardware page-table walk")
            pte = page_table.get(vpn)
            if pte is None:
                raise TranslationFault("invalid-PTE page fault", access, virtual_address)

            if len(self.order) == self.capacity:
                victim = self.order.pop(0)
                del self.entries[victim]
                log.append(
                    "[microarchitecture] TLB EVICT "
                    f"asid={victim[0]} vpn=0x{victim[1]:x}"
                )
            self.entries[key] = pte
            self.order.append(key)
            log.append(
                "[OS policy data] "
                f"PTE vpn=0x{vpn:x} -> ppn=0x{pte.ppn:x} "
                f"U={int(pte.user)} R={int(pte.read)} "
                f"W={int(pte.write)} X={int(pte.execute)}"
            )

        if not pte.permits(access, user_mode):
            raise TranslationFault("permission page fault", access, virtual_address)

        physical_address = pte.ppn * PAGE_SIZE + offset
        log.append(
            f"[architecture] translated PA 0x{physical_address:08x}"
        )
        return Translation(physical_address, cycles, hit)


class DirectMappedCache:
    """Tag-only direct-mapped cache sufficient for hit/miss observation."""

    def __init__(
        self,
        name: str,
        *,
        sets: int = CACHE_SETS,
        line_size: int = LINE_SIZE,
    ) -> None:
        if sets < 1 or line_size < 1:
            raise ValueError("cache geometry must be positive")
        self.name = name
        self.sets = sets
        self.line_size = line_size
        self.lines: list[int | None] = [None] * sets
        self.hits = 0
        self.misses = 0

    def access(
        self,
        physical_address: int,
        events: list[str] | None = None,
    ) -> CacheResult:
        if physical_address < 0:
            raise ValueError("physical address must be nonnegative")

        line_number = physical_address // self.line_size
        index = line_number % self.sets
        tag = line_number // self.sets
        old_tag = self.lines[index]
        hit = old_tag == tag
        log = events if events is not None else []

        if hit:
            self.hits += 1
            cycles = CACHE_HIT_CYCLES
            evicted = None
        else:
            self.misses += 1
            cycles = CACHE_FILL_CYCLES
            evicted = old_tag
            self.lines[index] = tag

        event = "HIT " if hit else "MISS"
        detail = (
            f"[microarchitecture] {self.name} {event} "
            f"pa=0x{physical_address:08x} set={index} tag=0x{tag:x}"
        )
        if evicted is not None:
            detail += f" evict_tag=0x{evicted:x}"
        log.append(detail)
        return CacheResult(cycles, hit, index, tag, evicted)


class MemorySystem:
    """Translation, protection, and cache path for one teaching core."""

    def __init__(
        self,
        page_table: dict[int, PTE] | None = None,
        *,
        tlb_capacity: int = 4,
    ) -> None:
        source = default_page_table() if page_table is None else page_table
        self.page_table = dict(source)
        self.tlb = TLB(tlb_capacity)
        self.icache = DirectMappedCache("I-cache")
        self.dcache = DirectMappedCache("D-cache")
        self.total_cycles = 0
        self.faults = 0

    def request(
        self,
        access: str,
        virtual_address: int,
        *,
        asid: int = 1,
        user_mode: bool = True,
    ) -> RequestResult:
        events = [f"[architecture] {access} VA 0x{virtual_address:08x}"]
        cache = self.icache if access == "fetch" else self.dcache

        try:
            translation = self.tlb.translate(
                virtual_address,
                access,
                self.page_table,
                asid=asid,
                user_mode=user_mode,
                events=events,
            )
            cache_result = cache.access(translation.physical_address, events)
            cycles = translation.cycles + cache_result.cycles
            self.total_cycles += cycles
            events.append(
                f"[microarchitecture] request completes; +{cycles} cycles"
            )
            return RequestResult(
                access,
                virtual_address,
                translation.physical_address,
                cycles,
                True,
                translation.tlb_hit,
                cache_result.hit,
                None,
                tuple(events),
            )
        except TranslationFault as fault:
            self.faults += 1
            self.total_cycles += FAULT_CYCLES
            events.append(f"[architecture] {fault}")
            events.append(
                "[OS policy] trap handler decides whether the task may resume"
            )
            return RequestResult(
                access,
                virtual_address,
                None,
                FAULT_CYCLES,
                False,
                None,
                None,
                fault.kind,
                tuple(events),
            )


def default_page_table() -> dict[int, PTE]:
    return {
        0x1: PTE(0x10, True, True, False, True),   # code: VA 0x1000
        0x4: PTE(0x20, True, True, False, False),  # read-only array
        0x7: PTE(0x30, True, True, True, False),   # writable data
    }


TRACE_REQUESTS = (
    ("fetch", 0x1000),  # TLB miss, I-cache miss
    ("fetch", 0x1004),  # TLB hit, same I-cache line hit
    ("load", 0x4000),   # TLB miss, D-cache miss
    ("load", 0x4004),   # TLB hit, same D-cache line hit
    ("store", 0x4004),  # TLB hit, but denied before cache access
    ("load", 0x9000),   # TLB miss, no valid PTE
)


def run_trace(
    requests: Iterable[tuple[str, int]] = TRACE_REQUESTS,
) -> tuple[MemorySystem, list[RequestResult]]:
    system = MemorySystem()
    results = [system.request(access, address) for access, address in requests]
    return system, results


def print_trace() -> None:
    system, results = run_trace()
    for result in results:
        print()
        print("\n".join(result.events))

    print(f"\n[observation] modeled total = {system.total_cycles} cycles")
    print(
        "[observation] "
        f"TLB hits={system.tlb.hits} misses={system.tlb.misses}; "
        f"I$ hits={system.icache.hits} misses={system.icache.misses}; "
        f"D$ hits={system.dcache.hits} misses={system.dcache.misses}; "
        f"faults={system.faults}"
    )
    print(
        "[limit] timing and policies are teaching parameters, not a real CPU"
    )


def check(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def selftest() -> None:
    system, results = run_trace()
    first_fetch, second_fetch, first_load, second_load, denied, absent = results

    check(first_fetch.completed, "first fetch should complete")
    check(first_fetch.tlb_hit is False, "first fetch should miss in TLB")
    check(first_fetch.cache_hit is False, "first fetch should miss in I-cache")
    check(first_fetch.physical_address == 0x10000, "wrong code translation")
    check(first_fetch.cycles == 15, "wrong cold-fetch cycle charge")

    check(second_fetch.tlb_hit is True, "second fetch should hit in TLB")
    check(second_fetch.cache_hit is True, "same-line fetch should hit")
    check(second_fetch.cycles == 2, "wrong warm-fetch cycle charge")

    check(first_load.physical_address == 0x20000, "wrong array translation")
    check(first_load.tlb_hit is False, "first array load should miss in TLB")
    check(first_load.cache_hit is False, "first array load should miss in D-cache")
    check(second_load.tlb_hit is True, "second array load should hit in TLB")
    check(second_load.cache_hit is True, "same-line array load should hit")

    dcache_accesses_before_faults = system.dcache.hits + system.dcache.misses
    check(not denied.completed, "read-only store should not complete")
    check(
        denied.fault_kind == "permission page fault",
        "store should report a permission page fault",
    )
    check(not absent.completed, "unmapped load should not complete")
    check(
        absent.fault_kind == "invalid-PTE page fault",
        "unmapped load should report an invalid-PTE page fault",
    )
    check(
        dcache_accesses_before_faults == 2,
        "faulting requests must not access the D-cache",
    )
    check(system.total_cycles == 46, "unexpected complete trace cost")
    print("PASS: translation, permission, cache, and fault trace")

    conflict_cache = DirectMappedCache("test-cache")
    a = conflict_cache.access(0x0000)
    b = conflict_cache.access(0x0040)
    c = conflict_cache.access(0x0000)
    check((a.hit, b.hit, c.hit) == (False, False, False), "conflict behavior")
    check(a.index == b.index == c.index == 0, "conflict set should match")
    check(b.evicted_tag == a.tag and c.evicted_tag == b.tag, "wrong victim tag")
    print("PASS: direct-mapped conflict replacement")

    tiny_tlb = TLB(capacity=1)
    table = default_page_table()
    tiny_tlb.translate(0x1000, "fetch", table)
    tiny_tlb.translate(0x4000, "load", table)
    check((1, 0x1) not in tiny_tlb.entries, "capacity-one TLB failed to evict")
    check((1, 0x4) in tiny_tlb.entries, "new translation was not installed")
    print("PASS: bounded TLB replacement")

    writable_table = default_page_table()
    writable_table[0x4] = PTE(0x20, True, True, True, False)
    writable = MemorySystem(writable_table)
    store = writable.request("store", 0x4004)
    check(store.completed, "write-enabled PTE should permit the store")
    check(store.physical_address == 0x20004, "wrong writable translation")
    print("PASS: OS permission change affects architectural outcome")

    asid_tlb = TLB(capacity=4)
    events: list[str] = []
    asid_tlb.translate(0x1000, "fetch", table, asid=1, events=events)
    asid_tlb.translate(0x1000, "fetch", table, asid=2, events=events)
    check(asid_tlb.misses == 2, "different ASIDs must not share one TLB match")
    print("PASS: address-space identity separates translations")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command",
        nargs="?",
        choices=("trace", "selftest"),
        default="trace",
        help="print the annotated trace or run deterministic checks",
    )
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    if args.command == "selftest":
        selftest()
    else:
        print_trace()


if __name__ == "__main__":
    main()
