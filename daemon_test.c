/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <sys/types.h>

static void handler(int signo)
{
	if (signo == SIGUSR1)
		exit(EXIT_SUCCESS);
	exit(EXIT_FAILURE);
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
	char buf[LONG_WIDTH];
	char *const target = "./daemon";
	char *const pid_str = pid2str(buf, LONG_WIDTH, getpid());
	const struct test {
		const char	*name;
		char		*const argv[5];
	} tests[] = {
		{
			.name	= "./daemon -p <parent pid>",
			.argv	= { target, "-p", pid_str, NULL},
		},
		{}, /* sentry */
	};
	const struct test *t;

	for (t = tests; t->name; t++) {
		const struct sigaction sa = { .sa_handler = handler };
		pid_t pid;
		int ret;

		ret = sigaction(SIGUSR1, &sa, NULL);
		if (ret == -1)
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
			if (pause() == -1)
				perror("pause");
	}
	return 0;
}
