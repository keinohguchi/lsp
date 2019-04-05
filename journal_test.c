/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./journal", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[6];
		int		want;
	} *t, tests[] = {
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "dump everything",
			.argv	= {target, NULL},
			.want	= 0,
		},
		{
			.name	= "dump systemd-journald log only",
			.argv	= {target, "-u", "systemd-journald", NULL},
			.want	= 0,
		},
		{
			.name	= "dump systemd-journald log only and keep the last cursor",
			.argv	= {target, "-u", "systemd-journald", "-f", ".cursor.txt", NULL},
			.want	= 0,
		},
		{
			.name	= "dump systemd-journald log only with 1024 single query",
			.argv	= {target, "-u", "systemd-journald", "-m", "1024", NULL},
			.want	= 0,
		},
		{.name = NULL}, /* sentry */
	};
	int ret = -1;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		ret = -1;
		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s: %s\n",
				t->name, strerror(errno));
			goto err;
		} else if (pid == 0) {
			ret = execv(target, t->argv);
			if (ret == -1) {
				fprintf(stderr, "%s: %s\n",
					t->name, strerror(errno));
				goto err;
			}
			/* not reachable */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			fprintf(stderr, "%s: %s\n",
				t->name, strerror(errno));
			goto err;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: killed by signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			goto err;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: does not exit\n",
				t->name);
			goto err;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected exit status:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			goto err;
		}
		ret = 0;
	}
err:
	if (target)
		free(target);
	if (ret)
		return 1;
	return 0;
}
