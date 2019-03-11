/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>

#ifndef NR_OPEN
#define NR_OPEN 1024
#endif /* NR_OPEN */

static const char *progname;
static struct parameter {
	unsigned	timeout;
	pid_t		ppid;
	int		family;
	int		type;
	int		proto;
	int		port;
} param = {
	.timeout	= 30,
	.ppid		= 0,
	.family		= PF_INET,
	.type		= SOCK_STREAM,
	.proto		= 0,
	.port		= 9999,
};
static const char *const opts = "t:P:h";
static const struct option lopts[] = {
	{"timeout",	required_argument,	NULL,	't'},
	{"ppid",	required_argument,	NULL,	'P'},
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
		case 'P':
			fprintf(stream, "\tparent PID(PPID) to report the server's PID\n");
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

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid) {
		int status = EXIT_SUCCESS;
		if (p->ppid) {
			/* report the child PID */
			const union sigval val = { .sival_int = (int)pid};
			ret = sigqueue(p->ppid, SIGCHLD, val);
			if (ret == -1) {
				perror("sigqueue(SIGCHLD)");
				status = EXIT_FAILURE;
			}
		}
		exit(status);
	}
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
		perror("open(/dev/null)");
		return -1;
	}
	ret = dup2(fd, STDOUT_FILENO);
	if (ret == -1) {
		perror("dup2(/dev/null, STDOUT_FILENO)");
		goto err;
	}
	ret = dup2(fd, STDERR_FILENO);
	if (ret == -1) {
		perror("dup2(/dev/null, STDERR_FILENO)");
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

static int init_timeout(const struct parameter *restrict p)
{
	unsigned timeout_remain = p->timeout;

	/* set the timeout alarm */
	alarm(0);
	while (timeout_remain)
		timeout_remain = alarm(timeout_remain);

	return 0;
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
	ret = init_timeout(p);
	if (ret == -1)
		return ret;
	return 0;
}

int main(int argc, char *argv[])
{
	struct parameter *p = &param;
	int ret, opt, status;
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
		case 'P':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= LONG_MAX)
				usage(stderr, EXIT_FAILURE);
			p->ppid = val;
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
	if (s != -1) {
		ret = -1;
		if (close(ret))
			perror("close");
	}
	/* signal the status to the process if specified */
	if (p->ppid) {
		const union sigval val = { .sival_int = ret ? 1 : 0 };
		ret = sigqueue(p->ppid, SIGCHLD, val);
		if (ret == -1) {
			perror("sigqueue");
			status = 1;
		}
	}
	/* wait for the timeout for now */
	while ((ret = pause()))
		if (errno == EINTR) {
			status = 0;
			break;
		}
out:
	status = 0;
	if (ret)
		status = 1;
	return status;
}
