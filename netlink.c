/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <getopt.h>
#include <poll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

struct context {
	const struct process	*p;
	struct pollfd		fds[1]; /* single file descriptor */
	nfds_t			nfds;
	struct msghdr		msg;
	struct sockaddr_nl	addr;
	struct iovec		iov;
	char			buf[8164];
};

static struct process {
	struct context		ctx[1];	/* single context */
	const char		*progname;
	short			timeout;
	int			type;
	int			family;
	int			group;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.timeout	= 5000, /* ms */
	.type		= SOCK_RAW,
	.family		= NETLINK_ROUTE,
	.group		= RTMGRP_LINK,
	.opts		= "t:T:f:g:h",
	.lopts		= {
		{"timeout",	required_argument,	NULL,	't'},
		{"type",	required_argument,	NULL,	'T'},
		{"family",	required_argument,	NULL,	'f'},
		{"group",	required_argument,	NULL,	'g'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0}, /* sentry */
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
		case 'T':
			fprintf(s, "\tNetlink socket type [raw|dgram] (default: raw)\n");
			break;
		case 'f':
			fprintf(s, "\tNetlink family [route] (default: route)\n");
			break;
		case 'g':
			fprintf(s, "\tNetlink address group [link] (default: link)\n");
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
	struct sockaddr_nl sa;
	int s, ret;

	s = socket(AF_NETLINK, p->type, p->family);
	if (s == -1) {
		perror("socket");
		return -1;
	}
	memset(&sa, 0, sizeof(sa));
	sa.nl_family = AF_NETLINK;
	sa.nl_groups = p->group;
	ret = bind(s, (struct sockaddr *)&sa, sizeof(sa));
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	/* initialize the message header */
	memset(&ctx->msg, 0, sizeof(ctx->msg));
	ctx->p			= p;
	ctx->msg.msg_name	= &ctx->addr;
	ctx->msg.msg_namelen	= sizeof(ctx->addr);
	ctx->msg.msg_iov	= &ctx->iov;
	ctx->msg.msg_iovlen	= 1;
	ctx->iov.iov_base	= ctx->buf;
	ctx->iov.iov_len	= sizeof(ctx->buf);
	ctx->nfds		= sizeof(ctx->fds)/sizeof(struct pollfd);
	ctx->fds[0].fd		= s;
	ctx->fds[0].events	= POLLIN;
	return 0;
err:
	if (close(s))
		perror("close");
	return ret;
}

static int fetch(struct context *ctx)
{
	int timeout = ctx->p->timeout;
	return poll(ctx->fds, ctx->nfds, timeout);
}

static int handle(struct context *ctx, int nr)
{
	int i;
	for (i = 0; i < ctx->nfds; i++) {
		if (ctx->fds[i].revents & POLLIN) {
			ssize_t len = recvmsg(ctx->fds[i].fd, &ctx->msg, 0);
			if (len == -1)
				perror("recvmsg");
			else {
				printf("%ld=recvmsg()\n", len);
				if (len == 0)
					return 0; /* EOF */
			}
		}
		if (ctx->fds[i].revents & ~POLLIN)
			printf("%d has events(0x%x)\n",
			       ctx->fds[i].fd, ctx->fds[i].revents&~POLLIN);
	}
	return i;
}

static void term(const struct process *restrict p)
{
	const struct context *ctx = p->ctx;
	const struct pollfd *fd;
	int i;

	for (i = 0, fd = ctx->fds; i < ctx->nfds; i++, fd++)
		if (fd->fd != -1)
			if (close(fd->fd))
				perror("close");
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
		case 'T':
			if (!strncasecmp(optarg, "raw", strlen(optarg)))
				p->type = SOCK_RAW;
			else if (!strncasecmp(optarg, "dgram", strlen(optarg)))
				p->type = SOCK_DGRAM;
			else
				usage(p, stderr, EXIT_FAILURE);
			break;
		case 'f':
			if (!strncasecmp(optarg, "route", strlen(optarg)))
				p->family = NETLINK_ROUTE;
			else
				usage(p, stderr, EXIT_FAILURE);
			break;
		case 'g':
			if (!strncasecmp(optarg, "link", strlen(optarg)))
				p->group |= RTMGRP_LINK;
			else
				usage(p, stderr, EXIT_FAILURE);
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
