/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>

static const char *progname;
static struct parameter {
	unsigned	timeout;
	int		family;
	int		type;
	int		proto;
	int		port;
} param = {
	.timeout	= 30,
	.family		= PF_INET,
	.type		= SOCK_STREAM,
	.proto		= 0,
	.port		= 9999,
};
static const char *const opts = "t:h";
static const struct option lopts[] = {
	{"timeout",	required_argument,	NULL,	't'},
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

static void timeout_action(int signo, siginfo_t *si, void *tcontext)
{
	exit(EXIT_SUCCESS);
}

static int init_signal(void)
{
	struct sigaction alrm = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= timeout_action,
	};
	int ret;

	ret = sigemptyset(&alrm.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		return ret;
	}
	ret = sigaddset(&alrm.sa_mask, SIGIO);
	if (ret == -1) {
		perror("sigaddset(SIGIO)");
		return ret;
	}
	ret = sigaction(SIGALRM, &alrm, NULL);
	if (ret == -1) {
		perror("sigaction(SIGALRM)");
		return ret;
	}
	return 0;
}

static int init(const struct parameter *restrict p)
{
	unsigned timeout_remain = p->timeout;
	int ret;

	ret = init_signal();
	if (ret == -1)
		return ret;
	/* set the timeout alarm */
	alarm(0);
	while (timeout_remain)
		timeout_remain = alarm(timeout_remain);
	return 0;
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
	s = socket(p->family, p->type, p->proto);
	while ((ret = pause()))
		if (errno == EINTR) {
			ret = 0;
			break;
		}
out:
	if (s != -1)
		if ((ret = close(ret)))
			perror("close");
	if (ret)
		return 1;
	return 0;
}
