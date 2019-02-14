/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>

/* program context */
static struct {
	long			sleep;
	const char		*progname;
	const char		*const version;
	const char		*const opts;
	const struct option	lopts[];
} ctx = {
	.sleep		= 10,
	.version	= "1.0.0",
	.opts		= "s:vh",
	.lopts		= {
		{"sleep",	required_argument,	NULL,	's'},
		{"version",	no_argument,		NULL,	'v'},
		{"help",	no_argument,		NULL,	'h'},
		{},
	},
};

static void version(FILE *stream)
{
	fprintf(stream, "%s version %s\n", ctx.progname, ctx.version);
	exit(EXIT_SUCCESS);
}

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", ctx.progname, ctx.opts);
	fprintf(stream, "options:\n");
	for (o = ctx.lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 's':
			fprintf(stream, "\torphan sleep period in second (default 10)\n");
			break;
		case 'v':
			fprintf(stream, "\toutput version information and exit\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this help and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *const argv[])
{
	pid_t pid;
	int opt;

	ctx.progname = argv[0];
	while ((opt = getopt_long(argc, argv, ctx.opts, ctx.lopts, NULL)) != -1) {
		long value;
		switch (opt) {
		case 's':
			value = strtol(optarg, NULL, 10);
			if (value == LONG_MIN || value == LONG_MAX) {
				perror("strtol");
				return 1;
			}
			ctx.sleep = value;
			break;
		case 'v':
			version(stdout);
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
	pid = fork();
	if (pid == -1) {
		perror("fork");
		return 1;
	} else if (pid == 0) {
		/* child */
		int i;
		for (i = 0; i < ctx.sleep; i++) {
			printf("child: ppid=%d\n", getppid());
			fflush(stdout);
			sleep(1);
		}
		printf("child: goodbye\n");
		return 0;
	}
	/* let the parent die before the child */
	printf("parent: pid=%d\n", getpid());
	printf("parent: goodbye\n");
	return 0;
}
