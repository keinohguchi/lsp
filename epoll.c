/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/epoll.h>

struct context {
	const struct process	*p;
	int			efd;
	struct epoll_event	events[1]; /* single event */
};

static struct process {
	struct context		ctx[1]; /* single context */
	short			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.timeout	= 5000,	/* ms */
	.opts		= "t:h",
	.lopts		= {
		{"timeout",	required_argument,	NULL,	't'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(s, "\tInactivity timeout in millisecond (default: %hd)\n",
				p->timeout);
			break;
		case 'h':
			fprintf(s, "\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int init(struct process *p)
{
	struct context *ctx = p->ctx;
	int efd, ret;

	efd = epoll_create1(EPOLL_CLOEXEC);
	if (efd == -1)
		return -1;
	ctx->p			= p;
	ctx->efd		= efd;
	if (!isatty(STDIN_FILENO))
		return 0;
	ctx->events[0].events	= EPOLLIN;
	ctx->events[0].data.fd	= STDIN_FILENO;
	ret = epoll_ctl(efd, EPOLL_CTL_ADD, STDIN_FILENO, ctx->events);
	if (ret == -1) {
		perror("epoll_ctl");
		goto err;
	}
	return 0;
err:
	if (close(efd))
		perror("close");
	return ret;
}

static void term(const struct process *restrict p)
{
	const struct context *const ctx = p->ctx;
	if (close(ctx->efd))
		perror("close");
}

static int fetch(struct context *ctx)
{
	int nr = sizeof(ctx->events)/sizeof(struct epoll_event);
	int timeout = ctx->p->timeout;
	printf("waiting...\n");
	return epoll_wait(ctx->efd, ctx->events, nr, timeout);
}

static int exec(struct context *ctx, int nr)
{
	struct epoll_event *e = ctx->events;
	char buf[BUFSIZ];
	int i;

	printf("handling...\n");
	if (nr == 0) {
		printf("epoll(2) timed out\n");
		return 0;
	}
	for (i = 0; i < nr; i++) {
		if (e->events & EPOLLIN) {
			ssize_t len = read(e->data.fd, buf, sizeof(buf));
			if (len == -1)
				perror("read");
			else {
				buf[len] = '\0';
				printf("%ld=read('%s')\n", len, buf);
				if (len == 0)
					return 0; /* EOF */
			}
		}
		if (e->events & EPOLLOUT)
			printf("%d is writable\n", e->data.fd);
	}
	return i;
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	int o, ret;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (o) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < -1 || val > SHRT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = val;
			break;
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = init(p);
	if (ret == -1)
		return 1;
	while ((ret = fetch(p->ctx)) != -1)
		if ((ret = exec(p->ctx, ret)) <= 0)
			break;
	term(p);
	if (ret)
		return 1;
	return 0;
}
