# SPDX-License-Identifier: GPL-2.0
PROGS := inode
all: $(PROGS)
	@for i in $(PROGS); do $(CC) -o $$i $${i}.c; done
.PHONY: clean
clean:
	@$(RM) a.out
	@for i in $(PROGS); do $(RM) -f $$i; done
