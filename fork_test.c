/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

int main(void)
{
	char *target = realpath("./fork", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[6];
		int		want;
	} *t, tests[] = {
		{
			.name	= "help short option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "help long option",
			.argv	= {target, "--help", NULL},
			.want	= 0,
		},
		{
			.name	= "version short option",
			.argv	= {target, "-v", NULL},
			.want	= 0,
		},
		{
			.name	= "version long option",
			.argv	= {target, "--version", NULL},
			.want	= 0,
		},
		{
			.name	= "missing option",
			.argv	= {target, "--missing", NULL},
			.want	= 1,
		},
		{
			.name	= "no argument, orphan sleep 10 seconds",
			.argv	= {target, NULL},
			.want	= 0,
		},
		{
			.name	= "orphan mode",
			.argv	= {target, "-m", "orphan", NULL},
			.want	= 0,
		},
		{
			.name	= "orphan sleep 2 seconds",
			.argv	= {target, "-s", "2", NULL},
			.want	= 0,
		},
		{
			.name	= "zombie mode with parent sleep 1 seconds",
			.argv	= {target, "-m", "zombi", "-s", "1", NULL},
			.want	= 0,
		},
		{},
	};
	int ret;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			abort();
		} else if (pid == 0) {
			/* child */
			ret = execv(t->argv[0], t->argv);
			if (ret == -1) {
				perror("execv");
				abort();
			}
			/* does not reach */
		}
		/* parent */
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			goto out;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: child exit with signal %s\n",
				t->name, strsignal(WTERMSIG(status)));
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: child didn't exit\n", t->name);
			goto out;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected result:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			goto out;
		}
	}
	ret = 0;
out:
	return ret;
}
