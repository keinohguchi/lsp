# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
PROGS += daemon
PROGS += affinity
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
.PHONY: run test clean
run: $(PROGS)
	@for i in $(PROGS); do if ! ./$$i; then exit 1; fi; done
test: $(TESTS)
$(TESTS): $(SRCS) $(TEST_SRCS)
	@printf "$@:\t"
	@if [ -f $@.c ]; then \
		$(CC) $(CFLAGS) -o $@ $(patsubst %_test.c,%.c,$@.c) $@.c;    \
		if ./$@; then echo PASS && true; else echo FAIL && false; fi \
	else echo "N/A"; fi
clean:
	@$(RM) $(PROGS) $(TESTS)
