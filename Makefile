# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
PROGS += daemon
PROGS += affinity
PROGS += resource
PROGS += thread
PROGS += withdraw
PROGS += xattr
PROGS += mstat
PROGS += clocks
PROGS += prime
PROGS += ls
PROGS += fork
PROGS += access
PROGS += time
PROGS += find
PROGS += id
PROGS += sh
PROGS += server
LIB       := liblsp.a
LIB_SRCS  := ls.c
LIB_OBJS  := $(patsubst %.c,%.o,$(LIB_SRCS))
TEST_SRCS := $(filter %_test.c,$(wildcard *.c))
TESTS     := $(patsubst %.c,%,$(TEST_SRCS))
CC     ?= gcc
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -g
CFLAGS += -lpthread
CFLAGS += -D_GNU_SOURCE
.PHONY: all help test check clean $(TESTS)
all: $(PROGS)
ls: ls_main.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^
sh: sh.o $(LIB)
	$(CC) $(CFLAGS) -o $@ $^
$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^
help: $(PROGS)
	@for i in $^; do if ! ./$$i --$@; then exit 1; fi; done
test check: $(TESTS)
$(TESTS): $(PROGS) $(TEST_SRCS)
	@$(CC) $(CFLAGS) -o $@ $@.c
	@printf "$@:\t"
	@if ./$@>/dev/null 2>&1; then\
		echo PASS;           \
	else                         \
		echo FAIL; exit 1;   \
	fi
clean:
	@$(RM) $(PROGS) $(TESTS)
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
