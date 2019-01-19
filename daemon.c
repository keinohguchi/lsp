/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <linux/limits.h>

static const char *progname;

static void usage(FILE *stream, int status, const char *const opts)
{
	fprintf(stream, "Usage: %s [-%s]\n", progname, opts);
	exit(status);
}

static int xdaemon(pid_t ppid)
{
	pid_t pid;
	int i;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid)
		/* let parent die to become daemon */
		exit(EXIT_SUCCESS);

	/* make it session leader/process group leader */
	if (setsid() == -1) {
		perror("setsid");
		return -1;
	}
	if (chdir("/") == -1) {
		perror("chdir");
		return -1;
	}
	/* close all the open files */
	for (i = 0; i < NR_OPEN; i++)
		close(i);
	/* stdin */
	if (open("/dev/null", O_RDWR) == -1) {
		perror("open");
		return -1;
	}
	/* stdout */
	if (dup2(0, 1) == -1) {
		perror("dup2");
		return -1;
	}
	/* stderr */
	if (dup2(0, 2) == -1) {
		perror("dup2");
		return -1;
	}
	return 0;
}

int main(int argc, char *const argv[])
{
	const char *const opts = "hp:";
	pid_t pid = 0;
	int ret;

	progname = argv[0];
	while ((ret = getopt(argc, argv, opts)) != -1) {
		switch (ret) {
		case 'p':
			ret = atoi(optarg);
			if (ret == -1)
				perror("atoi");
			pid = (pid_t)ret;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts);
			break;
		}
	}

	ret = xdaemon(pid);
	if (ret == -1)
		return 1;

	/* send signal to the process with -p option */
	if (pid) {
		/*
		 * signal handler can get pid from siginfo_t but
		 * just for fun.
		 */
		union sigval val = { .sival_int = getpid() };
		ret = sigqueue(pid, SIGUSR1, val);
		if (ret == -1)
			perror("sigqueue");
	}

	/* light the fire */
	for (;;) {
		/* just wait for the signal... */
		if (pause() == -1) {
			/*
			 * those should be logged instead
			 * of stdout/stderr, as it's daemon.
			 */
			if (errno != EINTR)
				perror("pause");
			fprintf(stderr, "interrupted...\n");
			exit(EXIT_SUCCESS);
		}
	}
}
