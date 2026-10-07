CC ?= cc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2
STRICT := -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
THREAD := -pthread
BIN := build
PROGRAMS := process_lab signal_lab proc_lab thread_lab queue_lab deadlock_lab litmus_lab
TARGETS := $(addprefix $(BIN)/,$(PROGRAMS))

.PHONY: all build test-week10 test-week11 test-all clean tools

all: build
build: $(TARGETS)

$(BIN)/.dir:
	mkdir -p $(BIN)
	touch $@

$(BIN)/process_lab: src/process_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/signal_lab: src/signal_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/proc_lab: src/proc_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/thread_lab: src/thread_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $(THREAD) $< -o $@

$(BIN)/queue_lab: src/queue_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $(THREAD) $< -o $@

$(BIN)/deadlock_lab: src/deadlock_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $(THREAD) $< -o $@

$(BIN)/litmus_lab: src/litmus_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $(THREAD) $< -o $@

test-week10: build
	./scripts/test-week10.sh

test-week11: build
	./scripts/test-week11.sh

test-all: test-week10 test-week11
	./scripts/optional-tools.sh

tools: build
	./scripts/optional-tools.sh

clean:
	rm -rf $(BIN)
