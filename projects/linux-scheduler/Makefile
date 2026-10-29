CC ?= cc
CPPFLAGS := -D_GNU_SOURCE
CFLAGS ?= -O2
STRICT := -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
BIN := build
PROGRAMS := scheduler_sim wakeup_lab switch_lab affinity_lab inversion_lab report_lab
TARGETS := $(addprefix $(BIN)/,$(PROGRAMS))
.PHONY: all build test-week15 tools check-source clean
all: build
build: $(TARGETS)
$(BIN)/.dir:
	mkdir -p $(BIN)
	touch $@
$(BIN)/%: src/%.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) -pthread $< -o $@
check-source:
	./scripts/check-source.py
test-week15: build check-source
	./scripts/test-week15.sh
tools: build
	./scripts/optional-tools.sh
clean:
	rm -rf $(BIN)
