SHELL := /bin/bash

CC := gcc
LD := ld
PROFILE ?= week6
MILESTONE ?= 6
FAULT ?= 0
BUILD := build/$(PROFILE)
ISO_ROOT := $(BUILD)/iso
KERNEL := $(BUILD)/toyos.elf
ISO := $(BUILD)/toyos.iso

C_SOURCES := src/kernel.c src/arch.c src/memory.c src/tasks.c
ASM_SOURCES := src/boot.S src/isr.S src/switch.S
OBJECTS := $(C_SOURCES:src/%.c=$(BUILD)/%.o) \
	$(ASM_SOURCES:src/%.S=$(BUILD)/%.o)

CPPFLAGS := -Iinclude -DCONFIG_MILESTONE=$(MILESTONE) -DEXPECT_FAULT=$(FAULT)
CFLAGS := -std=c11 -O2 -g3 -ffreestanding -fno-builtin -fno-pie \
	-fno-stack-protector -fno-asynchronous-unwind-tables -m64 \
	-mno-red-zone -mgeneral-regs-only -mcmodel=kernel \
	-Wall -Wextra -Wpedantic -Werror -Wconversion -Wshadow -Wundef
ASFLAGS := -g3 -ffreestanding -fno-pie -m64
LDFLAGS := -nostdlib -z max-page-size=0x1000 -T linker.ld
QEMU := qemu-system-x86_64 -machine accel=tcg -m 128M -display none \
	-serial stdio -monitor none -no-reboot \
	-device isa-debug-exit

.PHONY: all iso week6 week7 run-week6 run-week7 test-week6 test-week7 \
	debug-week6 debug-week7 gdb-week6 gdb-week7 \
	debug-fault-week6-page gdb-fault \
	expected-fault-week6-divide expected-fault-week6-page \
	expected-fault-week6-assert expected-fault-week7-page \
	expected-fault-week7-allocator expected-fault-week7-context clean

all: week6 week7

week6:
	@$(MAKE) --no-print-directory PROFILE=week6 MILESTONE=6 FAULT=0 iso

week7:
	@$(MAKE) --no-print-directory PROFILE=week7 MILESTONE=7 FAULT=0 iso

run-week6: week6
	@$(QEMU) -cdrom build/week6/toyos.iso

run-week7: week7
	@$(QEMU) -cdrom build/week7/toyos.iso

test-week6: week6
	@./scripts/check-qemu.sh build/week6/toyos.iso 33 "WEEK6: PASS"
	@$(MAKE) --no-print-directory expected-fault-week6-divide
	@$(MAKE) --no-print-directory expected-fault-week6-page
	@$(MAKE) --no-print-directory expected-fault-week6-assert

test-week7: week7
	@./scripts/check-qemu.sh build/week7/toyos.iso 33 "WEEK7: PASS"
	@$(MAKE) --no-print-directory expected-fault-week7-page
	@$(MAKE) --no-print-directory expected-fault-week7-allocator
	@$(MAKE) --no-print-directory expected-fault-week7-context

expected-fault-week6-divide:
	@$(MAKE) --no-print-directory PROFILE=week6-divide MILESTONE=6 FAULT=1 iso
	@./scripts/check-qemu.sh build/week6-divide/toyos.iso 35 \
		"EXPECTED-FAULT: divide PASS"

expected-fault-week6-page:
	@$(MAKE) --no-print-directory PROFILE=week6-page MILESTONE=6 FAULT=2 iso
	@./scripts/check-qemu.sh build/week6-page/toyos.iso 35 \
		"EXPECTED-FAULT: page PASS"

expected-fault-week6-assert:
	@$(MAKE) --no-print-directory PROFILE=week6-assert MILESTONE=6 FAULT=3 iso
	@./scripts/check-qemu.sh build/week6-assert/toyos.iso 37 \
		"EXPECTED-FAULT: assert PASS"

expected-fault-week7-page:
	@$(MAKE) --no-print-directory PROFILE=week7-page MILESTONE=7 FAULT=2 iso
	@./scripts/check-qemu.sh build/week7-page/toyos.iso 35 \
		"EXPECTED-FAULT: page PASS"

expected-fault-week7-allocator:
	@$(MAKE) --no-print-directory PROFILE=week7-allocator MILESTONE=7 FAULT=4 iso
	@./scripts/check-qemu.sh build/week7-allocator/toyos.iso 39 \
		"EXPECTED-FAULT: allocator PASS"

expected-fault-week7-context:
	@$(MAKE) --no-print-directory PROFILE=week7-context MILESTONE=7 FAULT=5 iso
	@./scripts/check-qemu.sh build/week7-context/toyos.iso 41 \
		"EXPECTED-FAULT: context PASS"

debug-week6: week6
	@echo "QEMU paused on :1234; run 'make gdb-week6' in another terminal."
	@$(QEMU) -S -s -cdrom build/week6/toyos.iso

debug-week7: week7
	@echo "QEMU paused on :1234; run 'make gdb-week7' in another terminal."
	@$(QEMU) -S -s -cdrom build/week7/toyos.iso

debug-fault-week6-page:
	@$(MAKE) --no-print-directory PROFILE=week6-page MILESTONE=6 FAULT=2 iso
	@echo "Fault VM paused on :1234; run 'make gdb-fault' in another terminal."
	@$(QEMU) -S -s -cdrom build/week6-page/toyos.iso

gdb-week6:
	@gdb -x gdb/week6.gdb

gdb-week7:
	@gdb -x gdb/week7.gdb

gdb-fault:
	@gdb -x gdb/fault.gdb

iso: $(ISO)

$(BUILD)/%.o: src/%.c include/toyos.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(KERNEL): $(OBJECTS) linker.ld
	$(LD) $(LDFLAGS) -Map $(BUILD)/toyos.map -o $@ $(OBJECTS)
	@grub2-file --is-x86-multiboot2 $@

$(ISO): $(KERNEL) grub.cfg
	@mkdir -p $(ISO_ROOT)/boot/grub
	@cp $(KERNEL) $(ISO_ROOT)/boot/toyos.elf
	@cp grub.cfg $(ISO_ROOT)/boot/grub/grub.cfg
	grub2-mkrescue -o $@ $(ISO_ROOT) >/dev/null 2>&1

clean:
	rm -rf build
