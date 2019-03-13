/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./httpd", NULL);
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
		{ .name = NULL },
	};
	int ret = 0;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			perror("fork");
			ret = -1;
			break;
		} else if (pid == 0) {
			/* child */
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				break;
			}
			/* not reach */
		}
		/* parent */
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			break;
		}
		ret = 0;
	}
	if (target)
		free(target);
	if (ret)
		return 1;
	return 0;
}
