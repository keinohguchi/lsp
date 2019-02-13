/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>

int main()
{
	char *const target = realpath("./ls", NULL);
	const struct test {
		const char	*name;
		char *const	argv[5];
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
			.name	= "reverse option",
			.argv	= {target, "-r", NULL},
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
			.name	= "all and list option",
			.argv	= {target, "-a", "-l", NULL},
			.want	= 0,
		},
		{
			.name	= "all, list, and reverse option",
			.argv	= {target, "-a", "-l", "-r", NULL},
			.want	= 0,
		},
		{
			.name	= "all and list combined option",
			.argv	= {target, "-al", NULL},
			.want	= 0,
		},
		{
			.name	= "all, list, and reverse combined option",
			.argv	= {target, "-alr", NULL},
			.want	= 0,
		},
		{
			.name	= "all and help option",
			.argv	= {target, "-a", "--help", NULL},
			.want	= 0,
		},
		{
			.name	= "version option",
			.argv	= {target, "--version", NULL},
			.want	= 0,
		},
		{
			.name	= "help option",
			.argv	= {target, "--help", NULL},
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
			.name	= "root directory",
			.argv	= {target, "/", NULL},
			.want	= 0,
		},
		{
			.name	= "list ls itself",
			.argv	= {target, "ls", NULL},
			.want	= 0,
		},
		{
			.name	= "non existent file",
			.argv	= {target, "some_borgus_file", NULL},
			.want	= 1,
		},
		{}, /* sentry */
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
		ret = 1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: unexpected signal: %s\n",
				t->name, strsignal(WTERMSIG(status)));
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: unexpected non exit\n",
				t->name);
			goto out;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr,
				"%s: unexpected result:\n- want: %d\n-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			goto out;
		}
		ret = 0;
	}
out:
	if (target)
		free(target);
	return ret;
}
