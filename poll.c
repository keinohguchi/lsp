/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <getopt.h>

struct context {
	const struct process	*p;
	struct pollfd		fds[1]; /* single file descriptor */
	nfds_t			nfds;
};

static struct process {
	struct context		ctx[1]; /* single context */
	short			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.ctx[0].nfds	= 0,
	.timeout	= 5000,
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
	return poll(ctx->fds, ctx->nfds, timeout);
}

static int handle(struct context *ctx)
{
	if (ctx->fds[0].revents & POLLIN)
		printf("stdin ready\n");
	return 0;
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

	/* main loop */
	while ((ret = fetch(p->ctx)) > 0)
		if ((ret = handle(p->ctx)))
			break;
	if (ret == 0)
		printf("poll(2) timed out\n");
	term(p);
	if (ret)
		return 1;
	return 0;
}
