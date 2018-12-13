# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
PROGS += daemon
SRCS  := $(wildcard *.c)
CC     := gcc
CFLAGS ?= -g
all: $(PROGS)
$(PROGS): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $@.c
.PHONY: clean
clean:
	@$(RM) $(PROGS)
