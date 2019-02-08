/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>

int main()
{
	char path[PATH_MAX];
	char *const target = realpath("./ls", path);
	const struct test {
		const char	*name;
		char *const	argv[4];
		int		want;
	} *t, tests[] = {
		{
			.name	= "all option",
			.argv	= {target, "-a", NULL},
			.want	= 0,
		},
		{
			.name	= "list option",
			.argv	= {target, "-l", NULL},
			.want	= 0,
		},
		{
			.name	= "list and all option",
			.argv	= {target, "-l", "-a", NULL},
			.want	= 0,
		},
		{
			.name	= "all and list option",
			.argv	= {target, "-a", "-l", NULL},
			.want	= 0,
		},
		{
			.name	= "all and help option",
			.argv	= {target, "-a", "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "no argument",
			.argv	= {target, NULL},
			.want	= 0,
		},
		{
			.name	= "current directory",
			.argv	= {target, ".", NULL},
			.want	= 0,
		},
		{
			.name	= "non existent file",
			.argv	= {target, "some_borgus_file", NULL},
			.want	= 1,
		},
		{}, /* sentry */
	};

	for (t = tests; t->name; t++) {
		int ret, status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			abort();
		} else if (pid == 0) {
			/* child */
			if (execv(t->argv[0], t->argv) == -1) {
				perror("execv");
				abort();
			}
			/* not reached */
		}
		/* parent */
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			abort();
		}
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: unexpected signal: %s\n",
				t->name, strsignal(WTERMSIG(status)));
			return 1;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: unexpected non exit\n",
				t->name);
			return 1;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr,
				"%s: unexpected result:\n- want: %d\n-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			return 1;
		}
	}
	return 0;
}
