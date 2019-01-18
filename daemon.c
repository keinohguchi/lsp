/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <linux/limits.h>

const char *progname;

static void usage(FILE *stream, int status)
{
	fprintf(stream, "Usage: %s [-hv]\n", progname);
	exit(status);
}

static void handler(int signo, siginfo_t *info, void *ctx)
{
	if (signo != SIGUSR1) {
		printf("oops\n");
		exit(EXIT_FAILURE);
	}
	printf("exiting...\n");
	exit(EXIT_SUCCESS);
}

static int xdaemon(pid_t ppid)
{
	union sigval val;
	pid_t pid;
	int i, ret;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid) {
		const struct sigaction act = {
			.sa_sigaction	= handler,
			.sa_flags	= SA_SIGINFO,
		};

		/* prepare to receive the daemon PID */
		ret = sigaction(SIGUSR1, &act, NULL);
		if (ret == -1)
			perror("sigaction");

		/* wait for the signal from the child and exit */
		ret = pause();
		if (ret == -1)
			perror("pause");
		/* no way */
		exit(EXIT_FAILURE);
	}

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
	/* send signal to the parent for the daemonization */
	val.sival_int = getpid();
	ret = sigqueue(ppid, SIGUSR1, val);
	if (ret == -1) {
		perror("sigqueue");
		return -1;
	}
	return 0;
}

int main(int argc, char *const argv[])
{
	const char *const opts = "hv";
	bool verbose = false;
	int ret, i;

	progname = argv[0];
	while ((ret = getopt(argc, argv, opts)) != -1) {
		switch (ret) {
		case 'v':
			verbose = true;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	if (verbose)
		for (i = optind; i < argc; i++)
			printf("argv[%d]=%s\n", i, argv[i]);

	ret = xdaemon(getpid());
	if (ret == -1)
		exit(EXIT_FAILURE);

	/* child */
	for (;;) {
		ret = pause();
		if (ret == -1)
			perror("pause");
	}
}
