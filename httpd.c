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
	struct sockaddr		*ss;
	struct sockaddr		*cs;
	socklen_t		slen;
};

/* process wide variables */
static struct process {
	const char		*progname;
	short			backlog;
	short			concurrent;
	int			domain;
	short			port;
	int			timeout;
	struct server		*servers;
	const char		*const opts;
	const struct option	lopts[];
} proc = {
	.backlog	= 5,
	.concurrent	= 2,
	.domain		= AF_INET,
	.port		= 80,
	.timeout	= 0,
	.opts		= "46b:c:p:t:h",
	.lopts		= {
		{"ipv4",	no_argument,		0,	'4'},
		{"ipv6",	no_argument,		0,	'6'},
		{"backlog",	required_argument,	0,	'b'},
		{"concurrent",	required_argument,	0,	'c'},
		{"port",	required_argument,	0,	'p'},
		{"timeout",	required_argument,	0,	't'},
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
		case '4':
			fprintf(s, "\t\tListen only on IPv4 (default)\n");
			break;
		case '6':
			fprintf(s, "\t\tListen only on IPv6\n");
			break;
		case 'b':
			fprintf(s, "\t\tListening backlog (default: %d)\n",
				p->backlog);
			break;
		case 'c':
			fprintf(s, "\tNumber of concurrent server(s) (default: %d)\n",
				p->concurrent);
			break;
		case 'p':
			fprintf(s, "\t\tListen on the port (default: %d)\n",
				p->port);
			break;
		case 't':
			fprintf(s, "\t\tProcess timeout in milliseconds (default: %d%s)\n",
				p->timeout, p->timeout > 0 ? "" : ", infinite");
			break;
		case 'h':
			fprintf(s, "\t\tdisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static void timeout_action(int signo, siginfo_t *sa, void *context)
{
	const struct process *restrict p = &proc;
	const struct server *s = p->servers;
	int i, ret, status = EXIT_SUCCESS;

	if (signo != SIGALRM)
		return;
	if (s == NULL)
		exit(status);
	for (i = 0; i < p->concurrent; i++) {
		if (s->tid == 0)
			continue;
		ret = pthread_cancel(s->tid);
		if (ret) {
			errno = ret;
			perror("pthread_cancel");
			status = EXIT_FAILURE;
		}
		s++;
	}
	exit(status);
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

static int term_server(struct server *ctx);

static int init_server(struct server *ctx)
{
	const struct process *const p = ctx->p;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	socklen_t slen = 0;
	int ret, opt, sd;

	ctx->sd = -1;
	switch (p->domain) {
	case AF_INET:
		slen = sizeof(struct sockaddr_in);
		sin = calloc(2, slen);
		if (sin == NULL) {
			perror("calloc");
			goto err;
		}
		sin[0].sin_family = sin[1].sin_family = AF_INET;
		sin[0].sin_addr.s_addr = sin[1].sin_addr.s_addr = 0;
		sin[0].sin_port = htons(p->port);
		ctx->ss = (struct sockaddr *)&sin[0];
		ctx->cs = (struct sockaddr *)&sin[1];
		ctx->slen = slen;
		break;
	case AF_INET6:
		slen = sizeof(struct sockaddr_in6);
		sin6 = calloc(2, slen);
		if (sin6 == NULL) {
			perror("calloc");
			goto err;
		}
		sin6[0].sin6_family = sin6[1].sin6_family = AF_INET6;
		memset(&sin6[0].sin6_addr, 0, sizeof(sin6->sin6_addr));
		memset(&sin6[1].sin6_addr, 0, sizeof(sin6->sin6_addr));
		sin6[0].sin6_port = sin6[1].sin6_port = htons(p->port);
		ctx->ss = (struct sockaddr *)&sin6[0];
		ctx->cs = (struct sockaddr *)&sin6[1];
		ctx->slen = slen;
		break;
	default:
		return -1;
	}
	sd = socket(p->domain, SOCK_STREAM, 0);
	if (sd == -1) {
		perror("socket");
		ret = -1;
		goto err;
	}
	ctx->sd = sd;
	opt = 1;
	ret = setsockopt(sd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));
	if (ret == -1) {
		perror("setsockopt(SO_REUSEPORT)");
		goto err;
	}
	ret = bind(sd, ctx->ss, slen);
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ret = listen(sd, p->backlog);
	if (ret == -1) {
		perror("listen");
		goto err;
	}
	return 0;
err:
	term_server(ctx);
	return ret;
}

static int term_server(struct server *ctx)
{
	int ret = 0;
	if (ctx->sd != -1)
		if (close(ctx->sd)) {
			perror("close");
			ret = -1;
		}
	if (ctx->ss)
		free(ctx->ss);
	return ret;
}

static void *server(void *arg)
{
	struct server *ctx = arg;
	socklen_t slen;
	int ret;

	ctx->status = EXIT_FAILURE;
	ret = init_server(ctx);
	if (ret == -1)
		return &ctx->status;
	ret = accept(ctx->sd, ctx->cs, &slen);
	if (ret == -1)
		perror("accept");
	else {
		if (close(ret))
			perror("close");
		else
			ctx->status = EXIT_SUCCESS;
	}
	ret = term_server(ctx);
	if (ret == -1)
		ctx->status = EXIT_FAILURE;
	return &ctx->status;
}

static int init(struct process *p)
{
	struct server *ss, *s;
	int i, j, ret;

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
	ret = init_signal(p);
	if (ret == -1)
		goto err;
	ret = init_timer(p);
	if (ret == -1)
		goto err;
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
	free(ss);
	return ret;
}

static void term(const struct process *restrict p)
{
	struct server *s = p->servers;
	int i, ret, *retp;

	if (s == NULL)
		return;
	for (i = 0; i < p->concurrent; i++) {
		if (s->tid == 0)
			continue;
		ret = pthread_kill(s->tid, 0);
		if (ret) {
			perror("pthread_kill");
			continue;
		}
		ret = pthread_join(s->tid, (void **)&retp);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			/* ignore error */
		}
		if (*retp != EXIT_SUCCESS)
			fprintf(stderr, "tid=%ld,status=%d\n", s->tid, *retp);
		s++;
	}
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
