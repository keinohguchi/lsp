/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./signal", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[4];
		int		signo;
		int		want;
	} *t, tests[] = {
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.signo	= 0,
			.want	= 0,
		},
		{
			.name	= "10ms sleep",
			.argv	= {target, "-t", "10", NULL},
			.signo	= SIGHUP,
			.want	= 0,
		},
		{.name = NULL}, /* sentry */
	};
	int ret = 0;

	for (t = tests; t->name; t++) {
		const union sigval si = {.sival_int = 0};
		int status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			return -1;
		} else if (pid == 0) {
			ret = execv(target, t->argv);
			if (ret == -1) {
				fprintf(stderr, "%s: %s",
					t->name, strerror(errno));
				exit(EXIT_FAILURE);
			}
		}
		ret = sigqueue(pid, t->signo, si);
		if (ret == -1) {
			fprintf(stderr, "%s: %s", t->name,
				strerror(errno));
			break;
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			fprintf(stderr, "%s: %s", t->name,
				strerror(errno));
			break;
		}
		ret = -1;
		if (t->signo != 0 && !WIFSIGNALED(status)) {
			fprintf(stderr, "%s: child does not signaled\n",
				t->name);
			break;
		}
		if (t->signo == 0 && !WIFEXITED(status)) {
			fprintf(stderr, "%s: child does not exit\n",
				t->name);
			break;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected result:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			break;
		}
		ret = 0;
	}
	if (target)
		free((void *)target);
	if (ret)
		return 1;
	return 0;
}
