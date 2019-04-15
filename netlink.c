/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <limits.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

struct context {
	const struct process	*p;
	int			sfd;
	int			efd;
	int			ifindex;
	struct epoll_event	events[1]; /* single event */
	struct msghdr		msg;
	struct sockaddr_nl	addr;
	struct iovec		iov;
	char			*buf;
};

static struct process {
	struct context		ctx[1];	/* single context */
	const char		*progname;
	int			edge:1;
	short			timeout;
	int			type;
	int			family;
	int			group;
	const char		*iface;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.edge		= 0,	/* level triggered */
	.timeout	= 5000, /* ms */
	.type		= SOCK_RAW,
	.family		= NETLINK_ROUTE,
	.group		= RTMGRP_LINK,
	.iface		= NULL,
	.opts		= "i:et:T:f:g:h",
	.lopts		= {
		{"edge",	no_argument,		NULL,	'e'},
		{"timeout",	required_argument,	NULL,	't'},
		{"type",	required_argument,	NULL,	'T'},
		{"family",	required_argument,	NULL,	'f'},
		{"group",	required_argument,	NULL,	'g'},
		{"iface",	required_argument,	NULL,	'i'},
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
		case 'i':
			fprintf(s, "\tInterface name to query for (default: none)");
			break;
		case 'e':
			fprintf(s, "\tEdge triggered events (default: off, e.g. level triggered)\n");
			break;
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

static void term(struct process *p);

static void term_action(int signo, siginfo_t *si, void *context)
{
	struct process *p = &process;
	if (signo != SIGTERM)
		return;
	term(p);
}

static int init_signal(const struct process *restrict p)
{
	struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= term_action,
	};
	int ret;

	ret = sigemptyset(&sa.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		return -1;
	}
	ret = sigaddset(&sa.sa_mask, SIGINT);
	if (ret == -1) {
		perror("sigaddset(SIGINT)");
		return -1;
	}
	ret = sigaction(SIGTERM, &sa, NULL);
	if (ret == -1) {
		perror("sigaction(SIGTERM)");
		return -1;
	}
	ret = sigemptyset(&sa.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		return -1;
	}
	ret = sigaddset(&sa.sa_mask, SIGTERM);
	if (ret == -1) {
		perror("sigaddset(SIGTERM)");
		return -1;
	}
	ret = sigaction(SIGINT, &sa, NULL);
	if (ret == -1) {
		perror("sigaction(SIGINT)");
		return -1;
	}
	return 0;
}

static int init_ifindex(const char *restrict iface)
{
	struct ifreq ifr;
	int sfd, ret;

	/* create AF_INET socket as older linux kernel
	 * AF_NETLINK doesn't support ioctl() */
	sfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sfd == -1) {
		perror("socket");
		return -1;
	}
	strcpy(ifr.ifr_name, iface);
	ret = ioctl(sfd, SIOCGIFINDEX, &ifr);
	if (ret == -1) {
		perror("ioctl");
		goto out;
	}
out:
	if (close(sfd))
		perror("close");
	if (ret)
		return ret;
	return ifr.ifr_ifindex;
}

static int init(struct process *p)
{
	struct context *ctx = p->ctx;
	int ret, efd = -1, sfd = -1;
	int ifindex;
	long bufsiz;

	efd = epoll_create1(EPOLL_CLOEXEC);
	if (efd == -1) {
		perror("epoll_create1");
		return -1;
	}
	if (!p->iface)
		ifindex = -1;
	else {
		ret = init_ifindex(p->iface);
		if (ret == -1)
			goto err;
		ifindex = ret;
	}
	sfd = socket(AF_NETLINK, p->type, p->family);
	if (sfd == -1) {
		perror("socket");
		goto err;
	}
	memset(&ctx->addr, 0, sizeof(struct sockaddr_nl));
	ctx->addr.nl_family = AF_NETLINK;
	ctx->addr.nl_groups = p->group;
	ret = bind(sfd, (struct sockaddr *)&ctx->addr, sizeof(ctx->addr));
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ctx->ifindex			= ifindex;
	ctx->events[0].events		= EPOLLIN;
	if (p->edge)
		ctx->events[0].events	|= EPOLLET;
	ctx->events[0].data.fd		= sfd;
	ret = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, ctx->events);
	if (ret == -1) {
		perror("epoll_ctl");
		goto err;
	}
	bufsiz = sysconf(_SC_PAGESIZE);
	if (bufsiz == -1) {
		perror("sysconf");
		goto err;
	}
	bufsiz *= 2; /* PAGESIZE*2 */
	ctx->buf = malloc(bufsiz);
	if (ctx->buf == NULL) {
		perror("malloc");
		goto err;
	}
	/* initialize the message header */
	memset(&ctx->msg, 0, sizeof(ctx->msg));
	ctx->msg.msg_name	= &ctx->addr;
	ctx->msg.msg_namelen	= sizeof(ctx->addr);
	ctx->msg.msg_iov	= &ctx->iov;
	ctx->msg.msg_iovlen	= 1;
	ctx->iov.iov_base	= ctx->buf;
	ctx->iov.iov_len	= bufsiz;
	ctx->p			= p;
	ctx->efd		= efd;
	ctx->sfd		= sfd;
	ret = init_signal(p);
	if (ret == -1)
		goto err;
	return 0;
err:
	if (sfd != -1)
		if (close(sfd))
			perror("close");
	if (efd != -1)
		if (close(efd))
			perror("close");
	return ret;
}

static int fetch(struct context *ctx)
{
	int timeout = ctx->p->timeout;
	int nr = sizeof(ctx->events)/sizeof(struct epoll_event);
	printf("waiting...\n");
	return epoll_wait(ctx->efd, ctx->events, nr, timeout);
}

static char *ifflags(unsigned flags)
{
	const struct flag {
		unsigned	flag;
		const char	*name;
	} *n, names[] = {
		{IFF_UP,		"UP"},
		{IFF_BROADCAST,		"BROADCAST"},
		{IFF_DEBUG,		"DEBUG"},
		{IFF_LOOPBACK,		"LOOPBACK"},
		{IFF_POINTOPOINT,	"POINTOPOINT"},
		{IFF_NOARP,		"NOARP"},
		{IFF_PROMISC,		"PROMISC"},
		{IFF_NOTRAILERS,	"NOTRAILERS"},
		{IFF_ALLMULTI,		"ALLMULTI"},
		{IFF_MASTER,		"MASTER"},
		{IFF_SLAVE,		"SLAVE"},
		{IFF_MULTICAST,		"MULTICAST"},
		{IFF_PORTSEL,		"PORTSEL"},
		{IFF_AUTOMEDIA,		"AUTOMEDIA"},
		{IFF_DYNAMIC,		"DYNAMIC"},
		{IFF_LOWER_UP,		"LOWER_UP"},
		{IFF_DORMANT,		"DORMANT"},
		{IFF_ECHO,		"ECHO"},
		{.name = NULL},
	};
	char *ptr, *buf = malloc(LINE_MAX);
	size_t len;

	if (buf == NULL)
		return NULL;
	len = LINE_MAX;
	ptr = buf;
	for (n = names; n->name; n++) {
		if (flags & n->flag) {
			int nr = snprintf(ptr, len-1, "%s%s",
					  ptr == buf ? "" : ",",
					  n->name);
			if (nr > 0) {
				ptr += nr;
				len -= nr;
			}
			if (len <= 0)
				break;
		}
	}
	return buf;
}

static int handle_ifinfomsg(struct context *ctx, const struct ifinfomsg *restrict ifi)
{
	char *flags;
	/* filter out based on the interface index */
	if (ctx->ifindex != -1 && ifi->ifi_index != ctx->ifindex)
		return 0;
	flags = ifflags(ifi->ifi_flags);
	if (flags == NULL) {
		printf("family=%d,type=%hd,index=%d,flags=0x%08x,change=0x%08x\n",
		       ifi->ifi_family, ifi->ifi_type, ifi->ifi_index,
		       ifi->ifi_flags, ifi->ifi_change);
		return 0;
	}
	printf("family=%d,type=%hd,index=%d,flags=%s,change=0x%08x\n",
	       ifi->ifi_family, ifi->ifi_type, ifi->ifi_index, flags,
	       ifi->ifi_change);
	free(flags);
	return 0;
}

static ssize_t handle_input_event(struct context *ctx, struct epoll_event *e)
{
	char *buf = ctx->buf;
	struct nlmsgerr *err;
	struct nlmsghdr *nh;
	ssize_t ret, len, total;
	int nr;

	ret = recvmsg(e->data.fd, &ctx->msg, 0);
	if (ret == -1) {
		perror("recvmsg");
		return  -1;
	} else if (ret == 0)
		return 0; /* EOF */
	total = len = ret;
	nr = 0;
	for (nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len)) {
		if (nh->nlmsg_type == NLMSG_DONE)
			break;
		switch (nh->nlmsg_type) {
		case NLMSG_NOOP:
			continue;
		case NLMSG_ERROR:
			err = NLMSG_DATA(nh);
			printf("nlmsgerr.error=%d\n", err->error);
			break;
		case RTM_NEWLINK:
		case RTM_DELLINK:
		case RTM_GETLINK:
			ret = handle_ifinfomsg(ctx, NLMSG_DATA(nh));
			if (ret == -1)
				return ret;
			nr++;
			break;
		default:
			printf("unsupported nlmsg_type(%d)\n", nh->nlmsg_type);
			break;
		}
	}
	printf("%ld=recvmsg() with %d nlmsg data\n", total, nr);
	return nr;
}

static ssize_t handle_event(struct context *ctx, struct epoll_event *e)
{
	ssize_t len = 0;
	if (e->events & EPOLLIN)
		len = handle_input_event(ctx, e);
	if (e->events & ~EPOLLIN)
		printf("%d has events(0x%x)\n",
		       e->data.fd, e->events&~EPOLLIN);
	return len;
}

static int handle(struct context *ctx, int nr)
{
	int i, ret;
	printf("handling...\n");
	if (nr == 0) {
		printf("epoll(2) timed out\n");
		return 0;
	}
	for (i = 0; i < nr; i++)
		if ((ret = handle_event(ctx, &ctx->events[i])) <= 0)
			return ret;
	return i;
}

static int request_ifinfomsg(struct context *ctx)
{
	struct nlmsghdr *nh = (struct nlmsghdr *)ctx->buf;
	struct ifinfomsg *ifi;
	size_t len;
	int ret;

	if (ctx->ifindex == -1)
		return 0;

	/* prepare the message */
	memset(nh, 0, sizeof(struct nlmsghdr)+sizeof(struct ifinfomsg));
	nh->nlmsg_type	= RTM_GETLINK;
	nh->nlmsg_flags	= NLM_F_REQUEST;
	nh->nlmsg_len	= sizeof(struct nlmsghdr)+sizeof(struct ifinfomsg);
	ifi		= (struct ifinfomsg *)(nh+1);
	ifi->ifi_family	= AF_UNSPEC;
	ifi->ifi_index	= ctx->ifindex;

	/* update the length */
	len = ctx->iov.iov_len;
	ctx->iov.iov_len = nh->nlmsg_len;

	ret = sendmsg(ctx->sfd, &ctx->msg, 0);
	if (ret == -1) {
		perror("sendmsg");
		/* ignore the error */
	}
	/* restore the iov length */
	ctx->iov.iov_len = len;
	return 0;
}

static void term(struct process *p)
{
	struct context *ctx = p->ctx;
	printf("terminating...\n");
	if (p->iface != NULL)
		free((void *)p->iface);
	if (ctx->buf != NULL)
		free(ctx->buf);
	if (ctx->efd != -1) {
		if (close(ctx->efd))
			perror("close");
		ctx->efd = -1;
	}
	if (ctx->sfd != -1) {
		if (close(ctx->sfd))
			perror("close");
		ctx->sfd = -1;
	}
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
		case 'i':
			if (p->iface)
				free((void *)p->iface);
			p->iface = strdup(optarg);
			break;
		case 'e':
			p->edge = 1;
			break;
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

	/* request the current interface status */
	ret = request_ifinfomsg(p->ctx);
	if (ret == -1)
		goto err;

	/* let's roll */
	while ((ret = fetch(p->ctx)) != -1)
		if ((ret = handle(p->ctx, ret)) <= 0)
			break;

err:
	term(p);
	if (ret)
		return 1;
	return 0;
}
