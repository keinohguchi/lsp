/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <poll.h>

struct context {
	const struct process	*p;
	struct pollfd		fds[1]; /* stdin */
	nfds_t			nfds;
};

static struct process {
	struct context		ctx[1]; /* single context */
	short			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.timeout	= 5000, /* ms */
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
	ctx->p			= p;
	ctx->fds[0].fd		= STDIN_FILENO;
	ctx->fds[0].events	= POLLIN;
	ctx->nfds = sizeof(ctx->fds)/sizeof(struct pollfd);
	return 0;
}

static void term(const struct process *restrict p)
{
	return;
}

static int fetch(struct context *ctx)
{
	int timeout = ctx->p->timeout;
	printf("waiting...\n");
	return poll(ctx->fds, ctx->nfds, timeout);
}

/* handle returns the number of handled events, or zero in case of timeout */
static int handle(const struct context *restrict ctx, int nr)
{
	char buf[BUFSIZ];
	int i, j;

	printf("handling...\n");
	if (nr == 0) {
		printf("poll(2) timed out");
		return 0;
	}
	j = 0;
	for (i = 0; i < ctx->nfds; i++) {
		if (!ctx->fds[i].revents)
			continue;
		switch (ctx->fds[i].fd) {
		case STDIN_FILENO:
			if (ctx->fds[i].revents & POLLIN) {
				ssize_t len = read(ctx->fds[i].fd, buf, sizeof(buf));
				if (len == -1)
					perror("read");
				else {
					buf[len] = '\0';
					printf("%ld=read('%s')\n", len, buf);
					if (len == 0)
						return 0; /* EOF */
				}
			}
			if (ctx->fds[i].revents & ~POLLIN)
				printf("stdin has other events(0x%x)\n",
				       ctx->fds[i].revents & ~POLLIN);
			break;
		default:
			printf("fd=%d has event(0x%x)\n",
			       ctx->fds[i].fd, ctx->fds[i].revents);
			break;
		}
		if (++j >= nr)
			break;
	}
	return j;
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	int ret, o;

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

	/* let's roll */
	while ((ret = fetch(p->ctx)) != -1)
		if ((ret = handle(p->ctx, ret)) <= 0)
			break;

	term(p);
	if (ret)
		return 1;
	return 0;
}
