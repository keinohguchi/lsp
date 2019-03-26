/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef NR_OPEN
#define NR_OPEN 1024
#endif /* NR_OPEN */

struct server {
	const struct process	*p;
	pid_t			pid;
	int			sd;
};

static struct process {
	int			daemon:1;
	unsigned		timeout;
	unsigned short		backlog;
	unsigned short		concurrent;
	int			family;
	int			type;
	int			proto;
	int			port;
	FILE			*output;
	struct server		*ss;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.daemon		= 0,
	.timeout	= 30000,
	.backlog	= 5,
	.concurrent	= 2,
	.family		= AF_INET,
	.type		= SOCK_STREAM,
	.proto		= 0,
	.port		= 9999,
	.ss		= NULL,
	.progname	= NULL,
	.opts		= "t:b:c:dsh",
	.lopts		= {
		{"timeout",	required_argument,	NULL,	't'},
		{"backlog",	required_argument,	NULL,	'b'},
		{"concurrent",	required_argument,	NULL,	'c'},
		{"daemon",	no_argument,		NULL,	'd'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *stream, int status)
{
	const struct option *o;

	fprintf(stream, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(stream, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(stream, "\t\tserver timeout in millisecond (default: %u)\n",
				p->timeout);
			break;
		case 'b':
			fprintf(stream, "\t\tserver listen backlog (default: %hu)\n",
				p->backlog);
			break;
		case 'c':
			fprintf(stream, "\tconcurrent servers (default: %hu)\n",
				p->concurrent);
			break;
		case 'd':
			fprintf(stream, "\t\tdaemonize the server\n");
			break;
		case 'h':
			fprintf(stream, "\t\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int init_daemon(const struct process *restrict p)
{
	int i, ret, fd = -1;
	pid_t pid;

	if (!p->daemon)
		return 0;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid)
		/* parent */
		exit(EXIT_SUCCESS);

	/* child */
	ret = setsid();
	if (ret == -1) {
		perror("setsid");
		goto err;
	}
	ret = chdir("/");
	if (ret == -1) {
		perror("chdir");
		goto err;
	}
	for (i = 0; i < NR_OPEN; i++)
		close(i);
	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	ret = dup(fd);
	if (ret == -1) {
		perror("dup");
		goto err;
	}
	ret = dup(fd);
	if (ret == -1) {
		perror("dup");
		goto err;
	}
	/* daemon now */
	return 0;
err:
	if (fd != -1)
		if (close(fd))
			perror("close(/dev/null)");
	return ret;
}

static void term(const struct process *restrict p);

static void timeout_action(int signo, siginfo_t *si, void *tcontext)
{
	const struct process *const p = &process;
	if (signo != SIGALRM)
		return;
	term(p);
	exit(EXIT_SUCCESS);
}

static int init_signal(const struct process *restrict p)
{
	struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= timeout_action,
	};
	int ret;

	/* nothing to do in case of no timeout */
	if (p->timeout == -1)
		return 0;

	/* for SIGALRM */
	ret = sigemptyset(&sa.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		return ret;
	}
	ret = sigaction(SIGALRM, &sa, NULL);
	if (ret == -1) {
		perror("sigaction(SIGALRM)");
		return ret;
	}
	return 0;
}

static int init_timer(const struct process *restrict p)
{
	const struct itimerval it = {
		.it_value = {
			.tv_sec		= p->timeout/1000,
			.tv_usec	= p->timeout%1000*1000,
		},
		.it_interval = {0, 0},
	};
	int ret;

	/* arm the new timer */
	ret = setitimer(ITIMER_REAL, &it, NULL);
	if (ret == -1) {
		perror("setitimer");
		return -1;
	}
	return 0;
}

static int reset_timer(const struct process *restrict p)
{
	const struct itimerval zero = {
		.it_value	= {0, 0},
		.it_interval	= {0, 0},
	};
	sigset_t mask;
	int ret;

	if (!p->timeout)
		return 1;

	ret = sigemptyset(&mask);
	if (ret == -1) {
		perror("sigemptymask");
		return -1;
	}
	ret = sigaddset(&mask, SIGALRM);
	if (ret == -1) {
		perror("sigaddset");
		return -1;
	}
	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	/* reset the timer */
	ret = setitimer(ITIMER_REAL, &zero, NULL);
	if (ret == -1) {
		perror("setitimer");
		/* ignore the error */
	}
	init_timer(p);
	ret = sigprocmask(SIG_UNBLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	return 0;
}

static int init_socket(struct server *ctx)
{
	const struct process *const p = ctx->p;
	struct sockaddr_in sin;
	int sd, ret, opt;

	sd = socket(p->family, p->type, p->proto);
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
	sin.sin_family = p->family;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(p->port);
	ret = bind(sd, (struct sockaddr *)&sin, sizeof(sin));
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ret = listen(sd, p->backlog);
	if (ret == -1) {
		perror("listen");
		goto err;
	}
	return sd;
err:
	if (sd != -1)
		if (close(sd))
			ret = -1;
	return ret;
}

static void dump(FILE *s, const unsigned char *restrict buf, size_t len)
{
	int i, j, width = 16;

	for (i = 0; i < len; i++) {
		fprintf(s, "%02x ", buf[i]);
		/* check if we are done for this line */
		if (i%width == (width-1) || i+1 == len) {
			/* fill the gap */
			for (j = (i%width); j < (width-1); j++)
				fprintf(s, "   ");
			/* ascii print */
			fprintf(s, "| ");
			for (j = i-(i%width); j <= i; j++)
				fprintf(s, "%c", isprint(buf[j]) ? buf[j] : '.');
			fprintf(s, "\n");
		}
	}
}

static void *server(void *arg)
{
	struct server *ctx = arg;
	const struct process *const p = ctx->p;
	socklen_t slen = sizeof(struct sockaddr_in);
	struct sockaddr_in sin;
	char buf[BUFSIZ];
	ssize_t len;
	int ret;

	ret = init_socket(ctx);
	if (ret == -1)
		return (void *)EXIT_FAILURE;
	ctx->sd = ret;
	for (;;) {
		int c = accept(ctx->sd, &sin, &slen);
		if (c == -1) {
			perror("accept");
			continue;
		}
		ret = snprintf(buf, sizeof(buf), "Hello world\n");
		if (ret < 0) {
			perror("snprintf");
			if (shutdown(c, SHUT_RDWR))
				perror("shutdown");
			continue;
		}
		len = send(c, buf, strlen(buf)+1, 0); /* +null */
		if (len == -1) {
			perror("send");
			if (shutdown(c, SHUT_RD))
				perror("shutdown");
			continue;
		}
again:
		len = recv(c, buf, sizeof(buf), 0);
		if (len == -1) {
			perror("recv");
			if (shutdown(c, SHUT_RDWR))
				perror("shutdown");
			continue;
		} else if (len == 0) {
			if (shutdown(c, SHUT_RDWR))
				perror("shutdown");
			continue;
		}
		reset_timer(p);
		dump(p->output, (unsigned char *)buf, len);
		len = send(c, buf, len, 0);
		if (len == -1) {
			perror("write");
			if (shutdown(c, SHUT_RDWR))
				perror("shutdown");
			continue;
		}
		goto again;
	}
	return NULL;
}

static int init_server(const struct process *restrict p)
{
	struct server *ss, *s;
	int i, j, ret, *retp;

	ss = calloc(p->concurrent, sizeof(struct server));
	if (ss == NULL) {
		perror("calloc");
		return -1;
	}
	s = ss;
	for (i = 0; i < p->concurrent; i++) {
		s->sd = -1;
		s->p = p;
		s->pid = fork();
		if (s->pid == -1) {
			perror("fork");
			goto err;
		} else if (s->pid == 0) {
			retp = server(s);
			if (retp != NULL)
				exit(EXIT_FAILURE);
			exit(EXIT_SUCCESS);
		}
		s++;
	}
	return 0;
err:
	for (j = 0, s = ss; j < i; j++, s++) {
		const union sigval val = {.sival_int = getpid()};
		int status;
		if (s->pid == -1)
			continue;
		ret = sigqueue(s->pid, SIGTERM, val);
		if (ret == -1) {
			perror("sigqueue");
			continue;
		}
		ret = waitpid(s->pid, &status, 0);
		if (ret == -1)
			perror("waitpid");
	}
	return -1;
}

static int init(const struct process *restrict p)
{
	int ret;

	ret = init_daemon(p);
	if (ret == -1)
		return ret;
	ret = init_server(p);
	if (ret == -1)
		return ret;
	ret = init_signal(p);
	if (ret == -1)
		return ret;
	return init_timer(p);
}

static void term(const struct process *const p)
{
	const union sigval val = {.sival_int = getpid()};
	struct server *s;
	int i, ret, status;

	for (i = 0, s = p->ss; i < p->concurrent && s; i++, s++) {
		if (s->pid == -1)
			continue;
		ret = sigqueue(s->pid, SIGTERM, val);
		if (ret == -1) {
			perror("sigqueue");
			continue;
		}
		ret = waitpid(s->pid, &status, 0);
		if (ret == -1)
			perror("waitpid");
	}
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	int ret, o;

	p->progname = argv[0];
	p->output = stdout;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (o) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val >= LONG_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = val;
			break;
		case 'b':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= LONG_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->backlog = val;
			break;
		case 'c':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= LONG_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->concurrent = val;
			break;
		case 'd':
			p->daemon = 1;
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
		goto out;
	pause();
out:
	term(p);
	if (ret)
		return 1;
	return 0;
}
