/* SPDX-License-Idetifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/wait.h>

static const char *prog = "withdraw";

static char *const targetpath(const char *const prog)
{
	static char buf[LINE_MAX];
	size_t len;
	char *endp;

	if (getcwd(buf, sizeof(buf)) == NULL) {
		perror("getcwd");
		abort();
	}
	len = strlen(buf);
	endp = &buf[len];
	if (snprintf(endp, LINE_MAX-len, "/%s", prog) < 0) {
		perror("snprintf");
		abort();
	}
	return buf;
}

int main(void)
{
	char *const target = targetpath(prog);
	const struct test {
		const char	*name;
		char		*const argv[8];
		long int	want;
	} *t, tests[] = {
		{
			.name	= "900 withdraw from 1000 deposit",
			.argv	= {target, "-d", "1000", "-w", "900", "-c", "1", NULL},
			.want	= 100,
		},
		{
			.name	= "9 100 withdraws from 1000 deposit",
			.argv	= {target, "-d", "1000", "-w", "100", "-c", "9", NULL},
			.want	= 100,
		},
		{
			.name	= "100 10 withdraws from 1000 deposit",
			.argv	= {target, "-d", "1000", "-w", "10", "-c", "100", NULL},
			.want	= 0,
		},
		{
			.name	= "1000 1 withdraws from 1000 deposit",
			.argv	= {target, "-d", "1000", "-w", "1", "-c", "1000", NULL},
			.want	= 0,
		},
		{}, /* sentry */
	};

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s: fork: %s\n",
				t->name, strerror(errno));
			abort();
		} else if (pid == 0) {
			if (execv(target, t->argv) == -1) {
				fprintf(stderr, "%s: execv: %s\n",
					t->name, strerror(errno));
				abort();
			}
			/* can't reach */
		}
		if (waitpid(pid, &status, 0) == -1) {
			fprintf(stderr, "%s: waitpid: %s\n",
				t->name, strerror(errno));
			abort();
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: cannot finish\n",
				t->name);
			return 1;
		}
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: got signaled with %s\n",
				t->name, strsignal(WSTOPSIG(status)));
			return 1;
		}
		if (WEXITSTATUS(status) != 0) {
			fprintf(stderr, "%s: failed with status=%d\n",
				t->name, WEXITSTATUS(status));
			return WEXITSTATUS(status);
		}
	}
	return 0;
}
