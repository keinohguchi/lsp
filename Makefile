# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
PROGS += daemon
#PROGS += affinity
#PROGS += resource
#PROGS += thread
#PROGS += withdraw
#PROGS += attribute
PROGS += mstat
PROGS += resolution
TESTS := $(patsubst %,%_test,$(PROGS))
TEST_SRCS := $(filter %_test.c,$(wildcard *.c))
CC     ?= gcc
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -g
CFLAGS += -lpthread
CFLAGS += -D_GNU_SOURCE
.PHONY: all run test check clean $(TESTS)
all: $(PROGS)
run: $(PROGS)
	@for i in $^; do if ! ./$$i; then exit 1; fi; done
test check: $(TESTS)
clean:
	@$(RM) $(PROGS) $(TESTS)
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
$(TESTS): $(PROGS) $(TEST_SRCS)
	@printf "$@:\t"
	@if [ -f $@.c ]; then \
		$(CC) $(CFLAGS) -o $@ $@.c;    \
		if ./$@; then echo PASS; else echo FAIL; exit 1; fi \
	else echo "N/A"; fi
