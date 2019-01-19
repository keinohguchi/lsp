/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

static pid_t daemon_pid;

/* retrieving the daemon's state */
static void handler(int signo, siginfo_t *si, void *ctx)
{
	/* let's figure out how to pass context... */
	fprintf(stderr, "ctx=%p\n", ctx);

	/* signal number */
	if (signo != SIGUSR1 || si->si_signo != SIGUSR1) {
		fprintf(stderr,
			"unexpected signal:\n- want: %s\n-  got: %s\n",
			sys_siglist[SIGUSR1], sys_siglist[signo]);
		exit(EXIT_FAILURE);
	}
	/* signal should be raised by sigqueue() */
	if (si->si_code != SI_QUEUE) {
		fprintf(stderr,
			"unexpected signal code:\n- want: %#x\n-  got: %#x\n",
			SI_QUEUE, si->si_code);
		exit(EXIT_FAILURE);
	}
	/* make sure sigval contains the daemon's pid */
	if (si->si_int != si->si_pid) {
		fprintf(stderr,
			"unexpected pid:\n- want: %d\n-  got: %d\n",
			si->si_pid, si->si_value.sival_int);
		exit(EXIT_FAILURE);
	}
	daemon_pid = si->si_int;
}

static char *const pid2str(char str[], size_t len, pid_t pid)
{
	int ret = snprintf(str, len, "%d", pid);
	if (ret == -1)
		perror("snprintf");
	return str;
}

int main(void)
{
	char buf[11]; /* size of pid_t for string */
	char *const pid_str = pid2str(buf, sizeof(buf), getpid());
	char *const target = "./daemon";
	const struct test {
		const char	*name;
		char		*const argv[5];
	} tests[] = {
		{
			.name	= "-p option",
			.argv	= { target, "-p", pid_str, NULL},
		},
		{}, /* sentry */
	};
	const struct test *t;

	for (t = tests; t->name; t++) {
		struct sigaction sa = {
			.sa_sigaction	= handler,
			.sa_flags	= SA_SIGINFO,
		};
		pid_t pid;
		int ret;

		if (sigfillset(&sa.sa_mask) == -1)
			perror("sigfillset");
		if (sigaction(SIGUSR1, &sa, NULL) == -1)
			perror("sigaction");

		pid = fork();
		if (pid == -1)
			perror("fork");
		else if (pid == 0) {
			ret = execv(target, t->argv);
			if (ret == -1)
				perror("execv");
			exit(EXIT_SUCCESS);
		}
		for (;;)
			if (pause() == -1) {
				if (errno != EINTR)
					perror("pause");
				fprintf(stderr, "interrupted by %d...\n",
					daemon_pid);
				break;
			}
	}
	return 0;
}
