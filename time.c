/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>

static const char *progname;
static const char *const opts = "h";
static const struct option lopts[] = {
	{"help",	no_argument,	NULL,	'h'},
	{NULL,		0,		NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] [command to run]\n", progname, opts);
	fprintf(stream, "options\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
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

static int rusage(char *const argv[])
{
	struct rusage ru;
	int status;
	pid_t pid;

	if ((pid = fork()) == -1) {
		perror("fork");
		return 1;
	} else if (pid == 0) {
		if (argv[0] == NULL)
			/* just exit with zero */
			return 0;
		if (execvp(argv[0], argv) == -1) {
			perror("execv");
			return 1;
		}
		/* not reachable */
	}
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid");
		return 1;
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "child terminated with %s\n",
			strsignal(WTERMSIG(status)));
		/* ignore */
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "child did not exit\n");
		/* ignore */
	}
	if (getrusage(RUSAGE_CHILDREN, &ru) == -1) {
		perror("getrusage");
		return 1;
	}
	/* print out the result */
	printf("%ld.%02lduser %ld.%02ldsystem ",
	       ru.ru_utime.tv_sec, ru.ru_utime.tv_usec/10000,
	       ru.ru_stime.tv_sec, ru.ru_stime.tv_usec/10000);
	printf("(%ldmaxresident)k\n",
	       ru.ru_maxrss);
	printf("%ldinputs+%ldoutputs (%ldmajor+%ldminor)pagefaults ",
	       ru.ru_inblock, ru.ru_oublock, ru.ru_majflt, ru.ru_minflt);
	printf("%ldswaps %ldsignals\n", ru.ru_nswap, ru.ru_nsignals);
	return WEXITSTATUS(status);
}

int main(int argc, char *argv[])
{
	int opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	if (optind >= argc)
		argv[optind] = NULL;
	return rusage(&argv[optind]);
}
