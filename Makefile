# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
PROGS += daemon
TESTS := $(patsubst %,%_test,$(PROGS))
SRCS  := $(filter-out %_test.c %_main.c,$(wildcard *.c))
OBJS  := $(patsubst %.c,%.o,$(SRCS))
MAIN_SRCS := $(filter %_main.c,$(wildcard *.c))
TEST_SRCS := $(filter %_test.c,$(wildcard *.c))
CC     := gcc
CFLAGS ?= -g
all: $(PROGS)
$(PROGS): $(SRCS) $(MAIN_SRCS)
	$(CC) $(CFLAGS) -o $@ $@.c $@_main.c
.PHONY: test clean
test: $(TESTS)
$(TESTS): $(SRCS) $(TEST_SRCS)
	@if [ -f $@.c ]; then \
		$(CC) $(CFLAGS) -o $@ $(patsubst %_test.c,%.c,$@.c) $@.c; \
		printf "$@:\t"; if ./$@; then echo PASS; else echo FAIL; fi \
	else printf "$@:\tN/A\n"; fi
clean:
	@$(RM) $(PROGS) $(TESTS)
