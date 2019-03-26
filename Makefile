# SPDX-License-Identifier: GPL-2.0
PROGS := select
PROGS += inode
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
PROGS += httpd
PROGS += netlink
LIB       := liblsp.a
LIB_SRCS  := ls.c
LIB_OBJS  := $(patsubst %.c,%.o,$(LIB_SRCS))
TEST_SRCS := $(filter %_test.c,$(wildcard *.c))
TESTS     := $(patsubst %.c,%,$(TEST_SRCS))
CC     ?= gcc
CFLAGS += -Wall
CFLAGS += -Werror
CFLAGS += -g
CFLAGS += -D_GNU_SOURCE
CFLAGS += -lpthread
CFLAGS += -lrt
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
	@log=$(shell mktemp .$@-XXXXXXX.log); \
	if ./$@>$$log 2>&1; then              \
		echo PASS;                    \
	else                                  \
		echo FAIL; cat $$log; exit 1; \
	fi
clean:
	@$(RM) $(PROGS) $(TESTS) .*.log
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
