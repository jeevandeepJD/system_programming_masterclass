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
ifneq ($(filter 8 9,$(MILESTONE)),)
C_SOURCES += src/process.c
ASM_SOURCES += src/user_entry.S src/user_image.S
endif
ifeq ($(MILESTONE),9)
C_SOURCES += src/vfs.c src/edu.c
endif
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

.PHONY: all iso week6 week7 week8 week9 \
	run-week6 run-week7 run-week8 run-week9 \
	test-week6 test-week7 test-week8 test-week9 test-all test-clang \
	debug-week6 debug-week7 debug-week8 debug-week9 \
	gdb-week6 gdb-week7 gdb-week8 gdb-week9 \
	debug-fault-week6-page gdb-fault \
	expected-fault-week6-divide expected-fault-week6-page \
	expected-fault-week6-assert expected-fault-week7-page \
	expected-fault-week7-allocator expected-fault-week7-context \
	expected-fault-week8-user-pointer expected-fault-week8-syscall \
	expected-fault-week9-filesystem expected-fault-week9-device clean

all: week6 week7 week8 week9

week6:
	@$(MAKE) --no-print-directory PROFILE=week6 MILESTONE=6 FAULT=0 iso

week7:
	@$(MAKE) --no-print-directory PROFILE=week7 MILESTONE=7 FAULT=0 iso

week8:
	@$(MAKE) --no-print-directory PROFILE=week8 MILESTONE=8 FAULT=0 iso

week9:
	@$(MAKE) --no-print-directory PROFILE=week9 MILESTONE=9 FAULT=0 iso

run-week6: week6
	@$(QEMU) -cdrom build/week6/toyos.iso

run-week7: week7
	@$(QEMU) -cdrom build/week7/toyos.iso

run-week8: week8
	@$(QEMU) -cdrom build/week8/toyos.iso

run-week9: week9
	@$(QEMU) -device edu -cdrom build/week9/toyos.iso

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

test-week8: week8
	@./scripts/check-qemu.sh build/week8/toyos.iso 33 "WEEK8: PASS"
	@$(MAKE) --no-print-directory expected-fault-week8-user-pointer
	@$(MAKE) --no-print-directory expected-fault-week8-syscall

test-week9: week9
	@./scripts/check-qemu.sh build/week9/toyos.iso 33 "WEEK9: PASS" -device edu
	@$(MAKE) --no-print-directory expected-fault-week9-filesystem
	@$(MAKE) --no-print-directory expected-fault-week9-device

test-all: test-week6 test-week7 test-week8 test-week9

test-clang:
	@$(MAKE) --no-print-directory clean
	@$(MAKE) --no-print-directory CC=clang test-all

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

expected-fault-week8-user-pointer:
	@$(MAKE) --no-print-directory PROFILE=week8-user-pointer MILESTONE=8 FAULT=6 iso
	@./scripts/check-qemu.sh build/week8-user-pointer/toyos.iso 43 \
		"EXPECTED-FAULT: user-pointer PASS"

expected-fault-week8-syscall:
	@$(MAKE) --no-print-directory PROFILE=week8-syscall MILESTONE=8 FAULT=7 iso
	@./scripts/check-qemu.sh build/week8-syscall/toyos.iso 45 \
		"EXPECTED-FAULT: syscall PASS"

expected-fault-week9-filesystem:
	@$(MAKE) --no-print-directory PROFILE=week9-filesystem MILESTONE=9 FAULT=8 iso
	@./scripts/check-qemu.sh build/week9-filesystem/toyos.iso 47 \
		"EXPECTED-FAULT: filesystem PASS" -device edu

expected-fault-week9-device:
	@$(MAKE) --no-print-directory PROFILE=week9-device MILESTONE=9 FAULT=9 iso
	@./scripts/check-qemu.sh build/week9-device/toyos.iso 49 \
		"EXPECTED-FAULT: device PASS" -device edu

debug-week6: week6
	@echo "QEMU paused on :1234; run 'make gdb-week6' in another terminal."
	@$(QEMU) -S -s -cdrom build/week6/toyos.iso

debug-week7: week7
	@echo "QEMU paused on :1234; run 'make gdb-week7' in another terminal."
	@$(QEMU) -S -s -cdrom build/week7/toyos.iso

debug-week8: week8
	@echo "QEMU paused on :1234; run 'make gdb-week8' in another terminal."
	@$(QEMU) -S -s -cdrom build/week8/toyos.iso

debug-week9: week9
	@echo "QEMU paused on :1234; run 'make gdb-week9' in another terminal."
	@$(QEMU) -device edu -S -s -cdrom build/week9/toyos.iso

debug-fault-week6-page:
	@$(MAKE) --no-print-directory PROFILE=week6-page MILESTONE=6 FAULT=2 iso
	@echo "Fault VM paused on :1234; run 'make gdb-fault' in another terminal."
	@$(QEMU) -S -s -cdrom build/week6-page/toyos.iso

gdb-week6:
	@gdb -x gdb/week6.gdb

gdb-week7:
	@gdb -x gdb/week7.gdb

gdb-week8:
	@gdb -x gdb/week8.gdb

gdb-week9:
	@gdb -x gdb/week9.gdb

gdb-fault:
	@gdb -x gdb/fault.gdb

iso: $(ISO)

$(BUILD)/%.o: src/%.c include/toyos.h
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: src/%.S
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(BUILD)/user.elf: $(BUILD)/user_program.o user.ld
	$(LD) -nostdlib -z max-page-size=0x1000 -T user.ld -o $@ $<

$(BUILD)/user_image.o: src/user_image.S $(BUILD)/user.elf
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(ASFLAGS) \
		-DUSER_IMAGE='"$(abspath $(BUILD)/user.elf)"' -c $< -o $@

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
