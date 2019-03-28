/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./select", NULL);
	const struct {
		char	*name;
		char	*const argv[4];
		int	want;
	} *t, tests[] = {
		{
			.name	= "-h option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "1ms timeout",
			.argv	= {target, "-t", "1", NULL},
			.want	= 0,
		},
		{ .name = NULL },
	};
	int ret;

	for (t = tests; t->name; t++) {
		int status, in[2];
		pid_t pid;

		ret = pipe(in);
		if (ret == -1) {
			perror("pipe");
			goto out;
		}
		ret = -1;
		pid = fork();
		if (pid == -1) {
			perror("fork");
			goto out;
		} else if (pid == 0) {
			ret = dup2(in[0], STDOUT_FILENO);
			if (ret == -1)
				exit(EXIT_FAILURE);
			ret = close(in[1]);
			if (ret == -1)
				exit(EXIT_FAILURE);
			ret = execv(target, t->argv);
			if (ret == -1)
				exit(EXIT_FAILURE);
			/* not reachable */
		}
		ret = close(in[0]);
		if (ret == -1) {
			perror("close");
			goto out;
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			goto out;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: child singaled(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: child did not exit\n",
				t->name);
			goto out;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected result:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			goto out;
		}
		ret = close(in[1]);
		if (ret == -1) {
			perror("close");
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
