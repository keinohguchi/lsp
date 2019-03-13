/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/wait.h>

static const char *progname;

/* my own system command */
static int xsystem(char *const argv[])
{
	int status;
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid == 0) {
		/* child */
		if (execv("/bin/sh", argv) == -1) {
			perror("execv");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
	/* parent */
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	} else if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return -1;
}

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] <command>\n", progname, opts);
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
	char **args;
	int opt, ret, i;

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
	if (optind >= argc)
		usage(stderr, EXIT_FAILURE, opts, lopts);

	/* allocate memory for the new command */
	args = calloc((argc-optind+2), sizeof(char *));
	if (!args) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	/* for execv(2) */
	args[0] = "sh";
	args[1] = "-c";
	for (i = 0; i < argc-1; i++)
		args[i+2] = argv[i+1];
	args[i+2] = NULL;
	ret = xsystem(args);
	free(args);
	return ret;
}
