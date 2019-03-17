/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

/* server context */
struct server {
	const struct process	*p;
	pthread_t		tid;
	int			status;
	int			sd;
	struct sockaddr		*sa;
};

/* process wide variables */
static struct process {
	const char		*progname;
	int			concurrent;
	int			timeout;
	int			domain;
	short			port;
	short			backlog;
	struct server		*servers;
	const char		*const opts;
	const struct option	lopts[];
} proc = {
	.backlog	= 5,
	.concurrent	= 2,
	.timeout	= 0,
	.domain		= AF_INET,
	.port		= 80,
	.opts		= "b:c:t:46p:h",
	.lopts		= {
		{"backlog",	required_argument,	0,	'b'},
		{"concurrent",	required_argument,	0,	'c'},
		{"timeout",	required_argument,	0,	't'},
		{"ipv4",	no_argument,		0,	'4'},
		{"ipv6",	no_argument,		0,	'6'},
		{"port",	required_argument,	0,	'p'},
		{"help",	no_argument,		0,	'h'},
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
		case 'b':
			fprintf(s, "\tListening backlog (default: %d)\n",
				p->backlog);
			break;
		case 'c':
			fprintf(s, "\tNumber of concurrent server(s) (default: %d)\n",
				p->concurrent);
			break;
		case 't':
			fprintf(s, "\tProcess timeout in milliseconds (default: %d%s)\n",
				p->timeout, p->timeout > 0 ? "" : ", no timeout");
			break;
		case '4':
			fprintf(s, "\tListen only on IPv4 (default)\n");
			break;
		case '6':
			fprintf(s, "\tListen only on IPv6\n");
			break;
		case 'p':
			fprintf(s, "\tListen on the port (default: %d)\n",
				p->port);
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

static void timeout_action(int signo, siginfo_t *sa, void *context)
{
	if (signo != SIGALRM)
		return;
	exit(EXIT_SUCCESS);
}

static int init_signal(const struct process *restrict p)
{
	struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= timeout_action,
	};
	int ret;

	ret = sigemptyset(&sa.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		return -1;
	}
	ret = sigaction(SIGALRM, &sa, NULL);
	if (ret == -1) {
		perror("sigaction");
		return -1;
	}
	return 0;
}

static int init_timer(const struct process *restrict p)
{
	const struct itimerval tv = {
		.it_interval.tv_sec	= 0,
		.it_interval.tv_usec	= 0,
		.it_value.tv_sec	= p->timeout/1000,
		.it_value.tv_usec	= (p->timeout%1000)*1000,
	};
	int ret;

	ret = setitimer(ITIMER_REAL, &tv, NULL);
	if (ret == -1) {
		perror("setitimer");
		return -1;
	}
	return 0;
}

static int init_server(struct server *ctx)
{
	const struct process *const p = ctx->p;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	socklen_t slen = 0;
	int ret, opt, sd = -1;

	switch (p->domain) {
	case AF_INET:
		slen = sizeof(struct sockaddr_in);
		sin = malloc(slen);
		if (sin == NULL) {
			perror("malloc");
			goto err;
		}
		sin->sin_family = AF_INET;
		sin->sin_addr.s_addr = 0;
		sin->sin_port = htons(p->port);
		ctx->sa = (struct sockaddr *)sin;
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
		sin6->sin6_port = htons(p->port);
		ctx->sa = (struct sockaddr *)sin6;
		break;
	}
	sd = socket(p->domain, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		ret = -1;
		goto err;
	}
	opt = 1;
	ret = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
	if (ret == -1) {
		perror("setsockopt(SO_REUSEPORT)");
		goto err;
	}
	ret = bind(sd, ctx->sa, slen);
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ret = listen(sd, p->backlog);
	if (ret == -1) {
		perror("listen");
		goto err;
	}
	ctx->sd = sd;
	return 0;
err:
	if (sd != -1)
		if (close(sd))
			perror("close");
	if (ctx->sa)
		free(ctx->sa);
	return ret;
}

static void term_server(struct server *ctx)
{
	if (ctx && ctx->tid) {
		int *retp;
		int ret = pthread_join(ctx->tid, (void **)&retp);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			/* ignore the error */
		}
		if (*retp != EXIT_SUCCESS)
			fprintf(stderr, "tid[%ld]: status=%d\n", ctx->tid, *retp);
	}
}

static void *server(void *arg)
{
	struct server *ctx = arg;
	int ret;

	ret = init_server(ctx);
	if (ret == -1) {
		ctx->status = EXIT_FAILURE;
		return &ctx->status;
	}
	ctx->status = EXIT_SUCCESS;
	if (close(ctx->sd)) {
		perror("close");
		ctx->status = EXIT_FAILURE;
	}
	return &ctx->status;
}

static int init(struct process *p)
{
	struct server *ss, *s;
	int i, j, ret;

	ret = init_signal(p);
	if (ret == -1)
		return -1;

	ret = init_timer(p);
	if (ret == -1)
		return -1;

	ss = calloc(p->concurrent, sizeof(struct server));
	if (ss == NULL) {
		perror("calloc");
		return -1;
	}
	memset(ss, 0, p->concurrent*sizeof(struct server));
	s = ss;
	for (i = 0; i < p->concurrent; i++) {
		s->p = p;
		ret = pthread_create(&s->tid, NULL, server, s);
		if (ret == -1)
			goto err;
		s++;
	}
	p->servers = ss;
	return 0;
err:
	s = ss;
	for (j = 0; j < i; j++) {
		ret = pthread_cancel(s->tid);
		if (ret) {
			errno = ret;
			perror("pthread_cancel");
			/* ignore the error */
		}
		s++;
	}
	return ret;
}

static void term(const struct process *restrict p)
{
	struct server *s = p->servers;
	int i;

	if (s == NULL)
		return;
	for (i = 0; i < p->concurrent; i++)
		term_server(s++);
	free(p->servers);
}

int main(int argc, char *const argv[])
{
	struct process *p = &proc;
	int ret, opt;

	p->progname = argv[0];
	optind = 0;
	while ((opt = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 'b':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val > UCHAR_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->backlog = val;
			break;
		case 'c':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val > SCHAR_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->concurrent = val;
			break;
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < -1 || val > UINT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = val;
			break;
		case '4':
			p->domain = AF_INET;
			break;
		case '6':
			p->domain = AF_INET6;
			break;
		case 'p':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= USHRT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->port = val;
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
	while (1)
		if (pause())
			break;
	term(p);
	return 0;
}
