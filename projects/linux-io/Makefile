CC ?= cc
CPPFLAGS := -D_GNU_SOURCE
CFLAGS ?= -O2
STRICT := -std=c17 -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
BIN := build
PROGRAMS := descriptor_lab stream_lab pagecache_lab pipe_lab readiness_lab mmap_lab debug_lab
TARGETS := $(addprefix $(BIN)/,$(PROGRAMS))

.PHONY: all build test-week12 tools clean

all: build
build: $(TARGETS)

$(BIN)/.dir:
	mkdir -p $(BIN)
	touch $@

$(BIN)/descriptor_lab: src/descriptor_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/stream_lab: src/stream_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/pagecache_lab: src/pagecache_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/pipe_lab: src/pipe_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/readiness_lab: src/readiness_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/mmap_lab: src/mmap_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

$(BIN)/debug_lab: src/debug_lab.c | $(BIN)/.dir
	$(CC) $(CPPFLAGS) $(CFLAGS) $(STRICT) $< -o $@

test-week12: build
	./scripts/test-week12.sh

tools: build
	./scripts/optional-tools.sh

clean:
	rm -rf $(BIN)
