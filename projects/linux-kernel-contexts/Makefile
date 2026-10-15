CC ?= cc
CPPFLAGS := -D_GNU_SOURCE
CFLAGS ?= -O2
STRICT := -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
BIN := build
PROGRAMS := syscall_lab fault_lab interrupts_lab analog_lab context_lab report_lab
TARGETS := $(addprefix $(BIN)/,$(PROGRAMS))

.PHONY: all build test-week14 tools clean

all: build
build: $(TARGETS)

$(BIN)/.dir:
	mkdir -p $(BIN)
	touch $@

$(BIN)/syscall_lab: src/syscall_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/fault_lab: src/fault_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/interrupts_lab: src/interrupts_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/analog_lab: src/analog_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/context_lab: src/context_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/report_lab: src/report_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

test-week14: build
	./scripts/test-week14.sh

tools: build
	./scripts/optional-tools.sh

clean:
	rm -rf $(BIN)
