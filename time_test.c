/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./time", NULL);
	const struct test {
		char	*name;
		char	*const argv[3];
		int	want;
	} *t, tests[] = {
		{
			.name	= "-h option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{}, /* sentry */
	};
	int ret;

	for (t = tests; t->name; t++) {
		int status, got;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			abort();
		} else if (pid == 0) {
			/* child */
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				abort();
			}
			/* not reached */
		}

		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			abort();
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: killed by signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: does not exit\n",
				t->name);
			goto out;
		}
		if ((got = WEXITSTATUS(status)) != t->want) {
			fprintf(stderr, "%s: unexpected status code:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, got);
			goto out;
		}
		ret = 0;
	}
out:
	if (target)
		free(target);
	if (ret)
		return 1;
	return 0;
}
