CC ?= cc
CPPFLAGS := -D_GNU_SOURCE
CFLAGS ?= -O2
STRICT := -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
BIN := build
PROGRAMS := address_lab fault_lab mapping_lab allocator_lab buddy_lab reclaim_lab debug_lab
TARGETS := $(addprefix $(BIN)/,$(PROGRAMS))

.PHONY: all build test-week13 tools clean

all: build
build: $(TARGETS)

$(BIN)/.dir:
	mkdir -p $(BIN)
	touch $@

$(BIN)/address_lab: src/address_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/fault_lab: src/fault_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/mapping_lab: src/mapping_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/allocator_lab: src/allocator_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/buddy_lab: src/buddy_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/reclaim_lab: src/reclaim_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/debug_lab: src/debug_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

test-week13: build
	./scripts/test-week13.sh

tools: build
	./scripts/optional-tools.sh

clean:
	rm -rf $(BIN)
