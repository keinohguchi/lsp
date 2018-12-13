# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
PROGS += system
CC     := gcc
CFLAGS ?= -g
all: $(PROGS)
$(PROGS):
	$(CC) $(CFLAGS) -o $@ $@.c
.PHONY: clean
clean:
	@$(RM) $(PROGS)
