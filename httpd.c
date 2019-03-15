/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

struct server {
	const struct context	*ctx;
	int			sd;
	struct sockaddr		*sa;
};

static struct context {
	const char		*progname;
	int			domain;
	short			port;
	short			backlog;
	struct server		*servers;
	const char		*const opts;
	const struct option	lopts[];
} context = {
	.domain		= AF_INET,
	.port		= 80,
	.backlog	= 5,
	.opts		= "46p:b:h",
	.lopts		= {
		{"ipv4",	no_argument,		0,	'4'},
		{"ipv6",	no_argument,		0,	'6'},
		{"port",	required_argument,	0,	'p'},
		{"backlog",	required_argument,	0,	'b'},
		{"help",	no_argument,		0,	'h'},
		{NULL, 0, NULL, 0}, /* sentry */
	},
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
		case 'p':
			fprintf(s, "\tListen on the port (default: %d)\n",
				ctx->port);
			break;
		case 'b':
			fprintf(s, "\tListening backlog (default: %d)\n",
				ctx->backlog);
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
	struct server *s = NULL;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	socklen_t slen = 0;
	int ret, opt, sd = -1;

	s = malloc(sizeof(struct server));
	if (s == NULL) {
		perror("malloc");
		return -1;
	}
	switch (ctx->domain) {
	case AF_INET:
		slen = sizeof(struct sockaddr_in);
		sin = malloc(slen);
		if (sin == NULL) {
			perror("malloc");
			goto err;
		}
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = 0;
		sin->sin_port = htons(ctx->port);
		s->sa = (struct sockaddr *)sin;
		break;
	case AF_INET6:
		slen = sizeof(struct sockaddr_in6);
		sin6 = malloc(slen);
		if (sin6 == NULL) {
			perror("malloc");
			goto err;
		}
		sin6->sin6_family = AF_INET6;
		memset(&sin6->sin6_addr, 0, sizeof(sin6->sin6_addr));
		sin6->sin6_port = htons(ctx->port);
		s->sa = (struct sockaddr *)sin6;
		break;
	}
	sd = socket(ctx->domain, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		ret = -1;
		goto err;
	}
	opt = 1;
	ret = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1) {
		perror("setsockopt(SO_REUSEADDR)");
		goto err;
	}
	ret = bind(sd, s->sa, slen);
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ret = listen(sd, ctx->backlog);
	if (ret == -1) {
		perror("listen");
		goto err;
	}
	ctx->servers = s;
	s->ctx = ctx;
	s->sd = sd;
	return 0;
err:
	if (sd != -1)
		if (close(sd))
			perror("close");
	if (s) {
		if (s->sa)
			free(s->sa);
		free(s);
	}
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
		long val;
		switch (opt) {
		case '4':
			ctx->domain = AF_INET;
			break;
		case '6':
			ctx->domain = AF_INET6;
			break;
		case 'p':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= 65535)
				usage(ctx, stderr, EXIT_FAILURE);
			ctx->port = val;
			break;
		case 'b':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= 65535)
				usage(ctx, stderr, EXIT_FAILURE);
			ctx->backlog = val;
			break;
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
