/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>

/* program context */
static struct {
	int			orphan:1;
	int			zombie:1;
	long			sleep;
	const char		*progname;
	const char		*const version;
	const char		*const opts;
	const struct option	lopts[];
} ctx = {
	.orphan		= 1,
	.zombie		= 0,
	.sleep		= 5,
	.version	= "1.0.1",
	.opts		= "m:s:vh",
	.lopts		= {
		{"mode",	required_argument,	NULL,	'm'},
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
		case 'm':
			fprintf(stream, "\tchild mode [orphan|zombie] (default:orphan)\n");
			break;
		case 's':
			fprintf(stream, "\tsleep length in second (default:5)\n");
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
	int opt, i;

	ctx.progname = argv[0];
	while ((opt = getopt_long(argc, argv, ctx.opts, ctx.lopts, NULL)) != -1) {
		long value;
		switch (opt) {
		case 'm':
			ctx.orphan = ctx.zombie = 0;
			if (!strncmp(optarg, "orphan", strlen(optarg)))
				ctx.orphan = 1;
			else if (!strncmp(optarg, "zombie", strlen(optarg)))
				ctx.zombie = 1;
			else
				usage(stdout, EXIT_FAILURE);
			break;
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
		if (ctx.orphan)
			for (i = 0; i < ctx.sleep; i++) {
				printf("child[%d,ppid:%d] zzz...\n", getpid(), getppid());
				fflush(stdout);
				sleep(1);
			}
		printf("child[%d,ppid:%d] goodbye\n", getpid(), getppid());
		return 0;
	}
	/* let the parent die before the child */
	if (ctx.zombie)
		for (i = 0; i < ctx.sleep; i++) {
			printf("parent[%d]: zzz...\n", getpid());
			fflush(stdout);
			sleep(1);
		}
	printf("parent[%d]: goodbye\n", getpid());
	return 0;
}
