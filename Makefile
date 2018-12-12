# SPDX-License-Identifier: GPL-2.0
PROGS := inode
PROGS += block
PROGS += wait
CFLAGS ?= -g
all: $(PROGS)
	@for i in $(PROGS); do $(CC) $(CFLAGS) -o $$i $${i}.c; done
.PHONY: clean
clean:
	@$(RM) $(PROGS)
