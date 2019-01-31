/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <getopt.h>

static const char *progname;

static void usage(FILE *stream, int status,
		  const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;

	fprintf(stream, "usage: %s [-%s] <nth prime>\n",
		progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t--%s,-%c:\t", o->name, o->val);
		switch (o->val) {
		case 'p':
			fprintf(stream, "parent PID\n");
			break;
		case 'h':
			fprintf(stream, "show this message\n");
			break;
		}
	}
	exit(status);
}

int main(int argc, char *const argv[])
{
	const struct option lopts[] = {
		{"help",	no_argument,		NULL,	'h'},
		{"ppid",	required_argument,	NULL,	'p'},
		{},
	};
	const char *const opts = "hp:";
	union sigval prime;
	pid_t ppid;
	long val;
	int ret, opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'p':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val >= LONG_MAX)
				usage(stderr, EXIT_FAILURE, opts, lopts);
			ppid = val;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	if (optind >= argc)
		usage(stderr, EXIT_FAILURE, opts, lopts);

	val = strtol(argv[optind++], NULL, 10);
	if (val < 1 || val >= LONG_MAX)
		usage(stderr, EXIT_FAILURE, opts, lopts);

	prime.sival_int = 0;
	ret = sigqueue(ppid, SIGUSR1, prime);
	if (ret == -1) {
		perror("sigqueue");
		return 1;
	}
	return 0;
}
