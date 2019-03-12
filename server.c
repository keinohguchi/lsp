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
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef NR_OPEN
#define NR_OPEN 1024
#endif /* NR_OPEN */

static const char *progname;
static struct parameter {
	unsigned	timeout;
	unsigned	backlog;
	int		daemon;
	int		pfamily;
	int		afamily;
	int		type;
	int		proto;
	int		port;
} param = {
	.timeout	= 30,
	.backlog	= 5,
	.daemon		= 0,
	.pfamily	= PF_INET,
	.afamily	= AF_INET,
	.type		= SOCK_STREAM,
	.proto		= 0,
	.port		= 9999,
};
static const char *const opts = "t:b:dh";
static const struct option lopts[] = {
	{"timeout",	required_argument,	NULL,	't'},
	{"backlog",	required_argument,	NULL,	'b'},
	{"daemon",	no_argument,		NULL,	'd'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct parameter *p = &param;
	const struct option *o;

	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(stream, "\tserver inactivity timeout (default: %us)\n",
				p->timeout);
			break;
		case 'b':
			fprintf(stream, "\tserver listen backlog (default: %d)\n",
				p->backlog);
			break;
		case 'd':
			fprintf(stream, "\tdaemonize the server\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int init_daemon(const struct parameter *restrict p)
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

static void timeout_action(int signo, siginfo_t *si, void *tcontext)
{
	if (signo != SIGALRM)
		return;
	exit(EXIT_SUCCESS);
}

static int init_signal(void)
{
	const struct parameter *p = &param;
	struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= timeout_action,
	};
	int ret;

	/* nothing to do in case of no timeout */
	if (!p->timeout)
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

static int init_timer(const struct parameter *restrict p)
{
	unsigned timeout_remain = p->timeout;

	/* set the timeout alarm */
	alarm(0);
	while (timeout_remain)
		timeout_remain = alarm(timeout_remain);

	return 0;
}

static int reset_timer(const struct parameter *restrict p)
{
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
	ret = init_timer(p);
	if (ret == -1)
		return -1;
	ret = sigprocmask(SIG_UNBLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	return 0;
}

static int init_socket(const struct parameter *restrict p)
{
	struct sockaddr_in sin;
	int s, ret, opt;

	s = socket(p->pfamily, p->type, p->proto);
	if (s == -1) {
		perror("socket");
		ret = -1;
		goto err;
	}
	opt = 1;
	ret = setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1) {
		perror("setsockopt(SO_REUSEADDR)");
		goto err;
	}
	sin.sin_family = p->afamily;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(p->port);
	ret = bind(s, (struct sockaddr *)&sin, sizeof(sin));
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ret = listen(s, p->backlog);
	if (ret == -1) {
		perror("listen");
		goto err;
	}
	return s;
err:
	if (s != -1)
		if (close(s))
			ret = -1;
	return ret;
}

static int init(const struct parameter *restrict p)
{
	int ret;

	ret = init_daemon(p);
	if (ret == -1)
		return ret;
	ret = init_signal();
	if (ret == -1)
		return ret;
	ret = init_timer(p);
	if (ret == -1)
		return ret;
	return init_socket(p);
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

int main(int argc, char *argv[])
{
	struct parameter *p = &param;
	int ret, opt;
	int s = -1;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val >= LONG_MAX)
				usage(stderr, EXIT_FAILURE);
			p->timeout = val;
			break;
		case 'b':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= LONG_MAX)
				usage(stderr, EXIT_FAILURE);
			p->backlog = val;
		case 'd':
			p->daemon = 1;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = init(p);
	if (ret == -1)
		goto out;

	/* light the fire */
	s = ret;
	while (1) {
		struct sockaddr_in client;
		socklen_t slen = sizeof(client);
		FILE *stream = stdout;
		char buf[BUFSIZ];
		int c;

		c = accept(s, (struct sockaddr *)&client, &slen);
		if (c == -1) {
			if (errno == EINTR) {
				/* timeout */
				ret = 0;
				break;
			}
			perror("accept");
			ret = -1;
			break;
		}
		fprintf(stream, "From %s:%d\n",
			inet_ntop(p->afamily, &client.sin_addr, buf, slen),
			ntohs(client.sin_port));
		ret = snprintf(buf, sizeof(buf), "Hello, World!\n");
		if (ret < 0)
			perror("snprintf");
		ret = send(c, buf, strlen(buf), 0);
		if (ret == -1)
			perror("send");
		ret = recv(c, buf, sizeof(buf), 0);
		if (ret == -1)
			perror("recv");
		while (ret > 0) {
			dump(stream, (unsigned char *)buf, ret);
			ret = reset_timer(p);
			if (ret == -1)
				goto client_close;
			ret = recv(c, buf, sizeof(buf), 0);
		}
client_close:
		if (close(c))
			perror("close");
	}
out:
	if (s != -1)
		if (close(s))
			perror("close");
	if (ret)
		return 1;
	return 0;
}
