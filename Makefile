# SPDX-License-Identifier: GPL-2.0
PROGS := select
PROGS += poll
PROGS += writev
PROGS += epoll
PROGS += inode
PROGS += block
PROGS += fork
PROGS += wait
PROGS += system
PROGS += daemon
PROGS += affinity
PROGS += resource
PROGS += thread
PROGS += withdraw
PROGS += xattr
PROGS += mstat
PROGS += signal
PROGS += clocks
PROGS += prime
PROGS += access
PROGS += time
PROGS += id
PROGS += ls
PROGS += find
PROGS += sh
PROGS += client
PROGS += server
PROGS += httpd
PROGS += netlink
PROGS += journal
OBJS  := $(patsubst %,%.o,$(PROGS))
LIB   := liblsp.a
#LIB  := liblsp.so
LIB_SRCS    := ls.c
LIB_OBJS    := $(patsubst %.c,%.o,$(LIB_SRCS))
TESTS_SRC   := $(filter %_test.c,$(wildcard *.c))
TESTS       ?= $(patsubst %.c,%,$(TESTS_SRC))
TESTS_GOSRC := $(filter %_test.go,$(wildcard *.go))
# Tests not ready on qemu/arm64 environment
TESTS_EXC := daemon_test
TESTS_EXC += thread_test
TESTS_EXC += withdraw_test
TESTS_EXC += ls_test
TESTS_EXC += find_test
TESTS_EXC += sh_test
TESTS_EXC += httpd_test
TESTS_EXC += netlink_test
TESTS_EXC += journal_test
QEMU    ?= /usr/bin/qemu-aarch64-static
CC      ?= gcc
CFLAGS  += -Wall
CFLAGS  += -Werror
CFLAGS  += -Werror=format-security
CFLAGS  += -g
CFLAGS  += -D_GNU_SOURCE
CFLAGS  += -fpic
LDFLAGS += -lpthread
LDFLAGS += -lrt
.PHONY: all help test check clean $(TESTS) $(TESTS_GOSRC)
all: $(PROGS)
$(filter-out ls sh journal,$(PROGS)):
	$(CC) $(CFLAGS) -o $@ $@.c $(LDFLAGS)
ls: ls_main.o $(LIB)
	$(CC) $(CFLAGS) -Wl,-rpath=. -o $@ $^ $(LDFLAGS)
sh: sh.o $(LIB)
	$(CC) $(CFLAGS) -Wl,-rpath=. -o $@ $^ $(LDFLAGS)
journal: journal.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lsystemd
$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^
	#$(CC) $(CFLAGS) -shared -o $@ $^
help: $(PROGS)
	@for i in $^; do if ! ./$$i --$@; then exit 1; fi; done
.PHONY: test $(TESTS) $(TESTS_GOSRC)
test: c-test go-test
c-test: $(TESTS)
$(TESTS): $(PROGS) $(TESTS_SRC)
	@$(CC) $(CFLAGS) -o $@ $@.c
	@printf "$@:\t"
	@log=$(shell mktemp .$@-XXXXXXX.log); \
	if ./$@>$$log 2>&1; then              \
		echo PASS;                    \
	else                                  \
		echo FAIL; cat $$log; exit 1; \
	fi
go-test: $(TESTS_GOSRC)
$(TESTS_GOSRC):
	@go test -v $@
clean:
	@-$(RM) $(OBJS) $(LIB) $(LIB_OBJS) $(PROGS) $(TESTS) go.sum *.o .*.log
%: %.c
	$(CC) $(CFLAGS) -o $@ $<
# Cross compilations through the docker container.
amd64: amd64-image
	docker run -v $(PWD):/home/build lsp/amd64 make all clean
arm64: arm64-image
	docker run -v $(PWD):/home/build -v $(QEMU):$(QEMU):ro lsp/$@ make all clean
%-amd64: amd64-image
	docker run -v $(PWD):/home/build lsp/amd64 make $* clean
%-arm64: arm64-image
	docker run -v $(PWD):/home/build -v $(QEMU):$(QEMU):ro \
		-e TESTS="$(filter-out $(TESTS_EXC),$(TESTS))" \
		lsp/arm64 make $* clean
%-image:
	if [ -x $(QEMU) ]; then cp $(QEMU) .; fi
	docker build -t lsp/$* -f Dockerfile.$* .
