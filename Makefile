# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
PROGS += daemon
PROGS += affinity
PROGS += resource
TESTS := $(patsubst %,%_test,$(PROGS))
SRCS  := $(filter-out %_test.c %_main.c,$(wildcard *.c))
OBJS  := $(patsubst %.c,%.o,$(SRCS))
MAIN_SRCS := $(filter %_main.c,$(wildcard *.c))
TEST_SRCS := $(filter %_test.c,$(wildcard *.c))
CC     := gcc
CFLAGS ?= -Wall -Werror -g
all: $(PROGS)
$(PROGS): $(OBJS) $(MAIN_SRCS)
	$(CC) $(CFLAGS) -o $@ $@.o $@_main.c
.PHONY: run test clean
run: $(PROGS)
	@for i in $(PROGS); do if ! ./$$i; then exit 1; fi; done
test: $(TESTS)
$(TESTS): $(OBJS) $(TEST_SRCS)
	@printf "$@:\t"
	@if [ -f $@.c ]; then \
		$(CC) $(CFLAGS) -o $@ $(patsubst %_test.c,%.o,$@.c) $@.c;    \
		if ./$@; then echo PASS && true; else echo FAIL && false; fi \
	else echo "N/A"; fi
clean:
	@$(RM) $(PROGS) $(OBJS) $(TESTS)
