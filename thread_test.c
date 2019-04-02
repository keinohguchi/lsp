/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./thread", NULL);
	const struct test {
		const char	*name;
		char		*const argv[4];
		int		signo;
		int		want;
	} *t, tests[] = {
		{
			.name	= "1 thread",
			.argv	= {target, "-c", "1", NULL},
			.signo	= 0,
			.want	= 0,
		},
		{
			.name	= "2 threads",
			.argv	= {target, "-c", "2", NULL},
			.want	= 0,
		},
		{
			.name	= "12 threads",
			.argv	= {target, "-c", "12", NULL},
			.want	= 0,
		},
		{
			.name	= "512 threads",
			.argv	= {target, "-c", "512", NULL},
			.want	= 0,
		},
		{
			.name	= "1024 threads",
			.argv	= {target, "-c", "1024", NULL},
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
			fprintf(stderr, "%s: fork: %s\n",
				t->name, strerror(errno));
			goto out;
		} else if (pid == 0) {
			ret = execv(t->argv[0], t->argv);
			if (ret == -1) {
				fprintf(stderr, "%s: execv: %s\n",
					t->name, strerror(errno));
				exit(EXIT_FAILURE);
			}
			/* not reached */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			fprintf(stderr, "%s: waitpid: %s\n",
				t->name, strerror(errno));
			goto out;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: got signal %s\n",
				t->name, strsignal(WTERMSIG(status)));
			goto out;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: won't exit\n", t->name);
			goto out;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected exit status:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
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
