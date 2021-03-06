/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>

static struct process {
	sig_atomic_t		signaled;
	long			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.signaled	= 0,
	.timeout	= 5000,
	.opts		= "t:h",
	.lopts		= {
		{"timeout",	required_argument,	NULL,	't'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
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
			fprintf(s, "\tTimeout in millisecond (default: %ld)\n",
				p->timeout);
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

static void hup_action(int signo, siginfo_t *si, void *ucontext)
{
	struct process *p = &process;
	if (signo != SIGHUP)
		return;
	p->signaled++;
}

static int msleep(long msec)
{
	struct timespec ts = {
		.tv_sec		= msec/1000,
		.tv_nsec	= msec%1000*1000*1000,
	};
	struct timespec rem;
	int ret;

	while ((ret = nanosleep(&ts, &rem) == -1)) {
		if (errno == EINTR) {
			ts = rem;
			continue;
		}
		perror("nanosleep");
		break;
	}
	return ret;
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	const struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= hup_action,
	};
	sigset_t mask, omask;
	int ret, o;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (o) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < -1 || val > LONG_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = val;
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
	ret = sigaction(SIGHUP, &sa, NULL);
	if (ret == -1) {
		perror("sigaction");
		return 1;
	}
	ret = sigemptyset(&mask);
	if (ret == -1) {
		perror("sigemptyset");
		return 1;
	}
	ret = sigaddset(&mask, SIGHUP);
	if (ret == -1) {
		perror("sigaddset");
		return 1;
	}
	ret = sigprocmask(SIG_SETMASK, &mask, &omask);
	if (ret == -1) {
		perror("sigprocmask");
		return 1;
	}
	fprintf(stderr, "signal blocked\n");
	msleep(p->timeout);
	ret = sigprocmask(SIG_SETMASK, &omask, NULL);
	fprintf(stderr, "signal unblocked\n");
	msleep(p->timeout);
	printf("%d signal(s) handled\n", p->signaled);
	return 0;
}
