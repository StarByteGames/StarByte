# StarByte Makefile
# Alternative build system to CMake. Usage:
#   make            # build release binary into build/starbyte
#   make debug      # debug build
#   make run        # build + run examples/demo.sb
#   make examples   # run all example .sb files
#   make install    # install to PREFIX (default /usr/local)
#   make clean

VERSION    := 0.3.0

CC         ?= cc
PREFIX     ?= /usr/local
BUILD_DIR  ?= build
BIN        := $(BUILD_DIR)/starbyte

SRC_DIR    := src
SRCS       := $(wildcard $(SRC_DIR)/*.c)
OBJS       := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS       := $(OBJS:.o=.d)

WARN       := -Wall -Wextra -Wpedantic -Wno-unused-parameter
CSTD       := -std=c11
DEFS       := -DSTARBYTE_VERSION=\"$(VERSION)\"

UNAME_S    := $(shell uname -s 2>/dev/null)
ifeq ($(OS),Windows_NT)
    DEFS  += -DSTARBYTE_WINDOWS=1
    LIBS  :=
else
    DEFS  += -DSTARBYTE_LINUX=1
    LIBS  := -lm
endif

OPT        ?= -O2
CFLAGS     ?= $(CSTD) $(WARN) $(OPT) $(DEFS) -I$(SRC_DIR) -MMD -MP
LDFLAGS    ?=

.PHONY: all debug release run examples clean install uninstall help

all: release

release: OPT := -O2
release: $(BIN)

debug: OPT := -O0 -g3
debug: clean-objs $(BIN)

$(BIN): $(OBJS) | $(BUILD_DIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

run: $(BIN)
	./$(BIN) examples/demo.sb

examples: $(BIN)
	@for f in examples/*.sb; do \
	    echo "=== $$f ==="; \
	    ./$(BIN) $$f || exit $$?; \
	done

install: $(BIN)
	install -Dm755 $(BIN) $(DESTDIR)$(PREFIX)/bin/starbyte

uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/starbyte

clean-objs:
	@rm -f $(OBJS) $(DEPS)

clean:
	@rm -rf $(BUILD_DIR)

help:
	@echo "Targets: all (default), debug, run, examples, install, uninstall, clean"
	@echo "Vars:    CC=$(CC) PREFIX=$(PREFIX) BUILD_DIR=$(BUILD_DIR) OPT=$(OPT)"

-include $(DEPS)
