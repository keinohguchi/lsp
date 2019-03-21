/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <sys/time.h>
#include <sys/select.h>

static struct process {
	long			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.timeout	= 5000, /* 5sec */
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
		fprintf(s, "\t-%c,--%s", o->val, o->name);
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

static int fetch(const struct process *restrict p, int fd)
{
	struct timeval tv = {
		.tv_sec		= p->timeout/1000,
		.tv_usec	= (p->timeout%1000)*1000,
	};
	fd_set rfd;
	int ret;

	/* select(2) requires resetting the sets */
again:
	FD_ZERO(&rfd);
	FD_SET(fd, &rfd);
	ret = select(fd+1, &rfd, NULL, NULL, &tv);
	if (ret == -1) {
		perror("select");
		return -1;
	} else if (ret == 0)
		/* timed out */
		return 0;

	if (!FD_ISSET(fd, &rfd))
		goto again;

	return ret;
}

static int handle(int fd)
{
	char buf[BUFSIZ+1];
	int ret;

	ret = read(fd, buf, BUFSIZ);
	if (ret == -1) {
		perror("read");
		return -1;
	}
	buf[ret] = '\0';
	printf("%s", buf);
	return 0;
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	int opt, ret;

	p->progname = argv[0];
	while ((opt = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val > LONG_MAX)
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

	/* let's rock */
	while ((ret = fetch(p, STDOUT_FILENO)) > 0)
		if ((ret = handle(STDOUT_FILENO)) < 0)
			break;

	if (ret < 0)
		return 1;
	return 0;
}
