/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/types.h>

static const char *progname;

/* let child send signum, if it's non zero and put the
 * result in status */
static int xwait(int ret, int *status)
{
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	}
	if (!pid) {
		/* child */
		if (ret)
			exit(ret);
		abort();
	}
	/* parent */
	return wait(status);
}

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] [exit code]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t--%s,-%c:\t", o->name, o->val);
		switch (o->val) {
		case 'h':
			fprintf(stream, "show this message\n");
			break;
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	const struct option lopts[] = {
		{"help",	no_argument,	NULL,	'h'},
		{NULL,		0,		NULL,	0},
	};
	const char *opts = "h";
	int opt, ret, status;
	pid_t pid;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	/* dump core in case of no exit status specified */
	if (optind >= argc)
		ret = 0;
	else
		ret = atoi(argv[optind++]);

	/*
	 * fork the child and let it exit with the ret
	 * as the exit status.  The parent, on the other
	 * hand, will wait for it and stores the child
	 * return value in status value, which means
	 * ret and status should be the same.
	 */
	pid = xwait(ret, &status);
	if (pid == -1)
		exit(EXIT_FAILURE);

	if (WIFEXITED(status))
		printf("Normal exit with exit status=%d\n",
		       WEXITSTATUS(status));
	if (WIFSIGNALED(status))
		printf("Signal received with signo=%d%s\n",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? "(core dumped)" : "");

	return 0;
}
