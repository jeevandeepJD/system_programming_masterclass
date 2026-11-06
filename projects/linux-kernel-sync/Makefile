CC ?= cc
CPPFLAGS := -D_POSIX_C_SOURCE=200809L
CFLAGS ?= -O2
STRICT := -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
BIN := build
PROGRAMS := primitive_select message_order spinlock_lab mutex_pi_lab refseq_lab epoch_rcu_lab debug_report
TARGETS := $(addprefix $(BIN)/,$(PROGRAMS))
.PHONY: all build test-week16 check-source tools clean
all: build
build: $(TARGETS)
$(BIN)/.dir:
	mkdir -p $(BIN)
	touch $@
$(BIN)/%: src/%.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) -pthread $< -o $@
check-source:
	./scripts/check-source.py
test-week16: build check-source
	./scripts/test-week16.sh
tools:
	./scripts/optional-tools.sh
clean:
	rm -rf $(BIN)
