# SPDX-License-Identifier: GPL-2.0
PROGS := inode block
CFLAGS ?= -g
all: $(PROGS)
	@for i in $(PROGS); do $(CC) $(CFLAGS) -o $$i $${i}.c; done
.PHONY: clean
clean:
	@$(RM) a.out
	@for i in $(PROGS); do $(RM) -f $$i; done
