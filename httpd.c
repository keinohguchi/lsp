/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>

static struct context {
	const char		*progname;
	int			domain;
	struct server		*servers;
	const char		*const opts;
	const struct option	lopts[];
} context = {
	.domain	= AF_INET,
	.opts	= "46h",
	.lopts	= {
		{"ipv4",	no_argument,	0,	'4'},
		{"ipv6",	no_argument,	0,	'6'},
		{"help",	no_argument,	0,	'h'},
		{NULL, 0, NULL, 0}, /* sentry */
	},
};

struct server {
	const struct context	*ctx;
	int			sd;
};

static void usage(const struct context *restrict ctx, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", ctx->progname, ctx->opts);
	fprintf(s, "options:\n");
	for (o = ctx->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case '4':
			fprintf(s, "\tListen only on IPv4 (default)\n");
			break;
		case '6':
			fprintf(s, "\tListen only on IPv6\n");
			break;
		case 'h':
			fprintf(s, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int init(struct context *ctx)
{
	struct server *s;
	int ret, sd;

	s = malloc(sizeof(struct server));
	if (s == NULL) {
		perror("malloc");
		return -1;
	}
	sd = socket(ctx->domain, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		ret = -1;
		goto err;
	}
	ctx->servers = s;
	s->ctx = ctx;
	s->sd = sd;
	return 0;
err:
	if (s)
		free(s);
	return ret;
}

static void term(struct context *ctx)
{
	struct server *s = ctx->servers;

	if (s && s->sd)
		if (close(s->sd))
			perror("close");
	if (s)
		free(s);
}

int main(int argc, char *const argv[])
{
	struct context *ctx = &context;
	int ret, opt;

	ctx->progname = argv[0];
	optind = 0;
	while ((opt = getopt_long(argc, argv, ctx->opts, ctx->lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(ctx, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(ctx, stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = init(ctx);
	if (ret == -1)
		return 1;
	term(ctx);
	return 0;
}
