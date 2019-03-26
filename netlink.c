/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <limits.h>
#include <poll.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

static struct process {
	const char		*progname;
	struct msghdr		msg;
	struct sockaddr_nl	addr;
	struct iovec		iov;
	char			buf[8164];
	struct pollfd		fds;
	nfds_t			nfds;
	short			timeout;
	int			type;
	int			family;
	int			group;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.fds.fd		= -1,
	.timeout	= -1,
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
	memset(&p->msg, 0, sizeof(p->msg));
	p->msg.msg_name		= &p->addr;
	p->msg.msg_namelen	= sizeof(p->addr);
	p->msg.msg_iov		= &p->iov;
	p->msg.msg_iovlen	= 1;
	p->iov.iov_base		= p->buf;
	p->iov.iov_len		= sizeof(p->buf);
	p->nfds = sizeof(p->fds)/sizeof(struct pollfd);
	p->fds.fd = s;
	p->fds.events = POLLIN;
	return 0;
err:
	if (close(s))
		perror("close");
	return ret;
}

static const struct msghdr *fetch(struct process *p)
{
	ssize_t len;

	while (1) {
		int ret = poll(&p->fds, p->nfds, p->timeout);
		if (ret == -1) {
			perror("poll");
			return NULL;
		}
		if (ret == 0)
			return NULL;
		if (p->fds.revents & POLLIN)
			break;
	}
	len = recvmsg(p->fds.fd, &p->msg, 0);
	if (len == -1) {
		perror("recvmsg");
		return NULL;
	} else if (len == 0)
		return NULL;
	printf("%ld=recvmsg()\n", len);
	return &p->msg;
}

static int handle(const struct msghdr *restrict msg)
{
	char *buf = msg->msg_iov->iov_base;
	printf("handling %c...\n", *buf);
	return 0;
}

static void term(const struct process *restrict p)
{
	const struct pollfd *fd;
	int i;

	for (i = 0, fd = &p->fds; i < p->nfds; i++, fd++)
		if (fd->fd != -1)
			if (close(fd->fd))
				perror("close");
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	const struct msghdr *msg;
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

	/* handle the netlink messages */
	while ((msg = fetch(p)))
		if (handle(msg))
			break;

	term(p);
	return 0;
}
