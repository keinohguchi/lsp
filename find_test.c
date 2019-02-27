/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./find", NULL);
	const struct test {
		char	*name;
		char	*argv[4];
		int	want;
	} *t, tests[] = {
		{
			.name	= "name option",
			.argv	= {target, "-n", "test", NULL},
			.want	= 0,
		},
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{},
	};
	int ret;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		ret = -1;
		pid = fork();
		if (pid == -1) {
			perror("fork");
			goto out;
		} else if (pid == 0) {
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				goto out;
			}
			/* not reachable */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			goto out;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: signaled(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: not exit\n", t->name);
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
	if (target)
		free(target);
	if (ret)
		return 1;
	return ret;
}
