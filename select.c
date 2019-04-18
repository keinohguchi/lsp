/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/select.h>

struct context {
	const struct process	*p;
	fd_set			rfds, rrfds;
	fd_set			wfds, rwfds;
	fd_set			xfds, rxfds;
	int			nfds;
};

static struct process {
	struct context		ctx[1]; /* single context */
	long			timeout;
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
		fprintf(s, "\t-%c,--%s", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(s, "\tTimeout in millisecond (default: %ld)\n",
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
	ctx->p = p;
	FD_ZERO(&ctx->rfds);
	FD_ZERO(&ctx->wfds);
	FD_ZERO(&ctx->xfds);
	ctx->nfds = 0;
	FD_SET(STDIN_FILENO, &ctx->rfds);
	if (STDIN_FILENO >= ctx->nfds)
		ctx->nfds = STDIN_FILENO+1;
	return 0;
}

static void term(const struct process *restrict p)
{
	return;
}

static int fetch(struct context *ctx)
{
	struct timeval timeout = {
		.tv_sec		= ctx->p->timeout/1000,
		.tv_usec	= ctx->p->timeout%1000*1000,
	};

	printf("waiting...\n");
	/* select(2) requires to reset the fd_set for each call */
	ctx->rrfds = ctx->rfds;
	ctx->rwfds = ctx->wfds;
	ctx->rxfds = ctx->xfds;
	return select(ctx->nfds, &ctx->rfds, &ctx->wfds, &ctx->xfds,
		      &timeout);
}

static int exec(const struct context *restrict ctx, int nr)
{
	char buf[BUFSIZ];
	int i, j;

	printf("handling...\n");
	if (nr == 0) {
		printf("select(2) timed out\n");
		return 0;
	}
	for (i = 0; i < nr; ) {
		for (j = 0; j < ctx->nfds; j++)
			if (FD_ISSET(j, &ctx->rrfds)) {
				ssize_t len = read(j, buf, sizeof(buf));
				if (len == -1)
					perror("read");
				else {
					buf[len] = '\0';
					printf("%ld=read('%s')\n", len, buf);
					if (len == 0)
						return 0; /* EOF */
				}
				i++;
			}
		for (j = 0; j < ctx->nfds; j++)
			if (FD_ISSET(j, &ctx->rwfds)) {
				printf("fileno(%d) is write ready\n", j);
				i++;
			}
		for (j = 0; j < ctx->nfds; j++)
			if (FD_ISSET(j, &ctx->rxfds)) {
				printf("fileno(%d) is exception ready\n", j);
				i++;
			}
	}
	return i;
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	int opt, ret;

	p->progname = argv[0];
	while ((opt = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val > LONG_MAX)
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
		if ((ret = exec(p->ctx, ret)) <= 0)
			break;

	term(p);
	if (ret < 0)
		return 1;
	return 0;
}
