/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./access", NULL);
	const struct test {
		char	*name;
		char	*const argv[5];
		int	want;
	} *t, tests[] = {
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "target itself",
			.argv	= {target, target, NULL},
			.want	= 0,
		},
		{
			.name	= "bogus file",
			.argv	= {target, "something_bogus_file.txt", NULL},
			.want	= 1,
		},
		{ .name	= NULL }, /* sentry */
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
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				abort();
			}
			/* don't reach */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			abort();
		}
		ret = 1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: child is signaled\n", t->name);
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: child did not exit\n", t->name);
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
	return ret;
}
