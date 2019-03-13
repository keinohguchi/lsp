/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sched.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>

static char *const targetpath(const char *const target)
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
	if (snprintf(endp, LINE_MAX-len, "/%s", target) < 0) {
		perror("snprintf");
		abort();
	}
	return buf;
}

int main(void)
{
	char *target = targetpath("affinity");
	const struct test {
		char	*name;
		char	*const argv[5];
	} tests[] = {
		{
			.name	= "get CPU affinity",
			.argv	= { target, NULL },
		},
		{
			.name	= "set CPU0 affinity",
			.argv	= { target, "-c", "0", "-l", NULL },
		},
		{
			.name	= "set CPU1 affinity",
			.argv	= { target, "-c", "1", "-l", NULL },
		},
		{ .name = NULL }, /* sentry */
	};
	const struct test *t;

	for (t = tests; t->name; t++) {
		int status, ret;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s: fork: %s\n",
				t->name, strerror(errno));
			abort();
		} else if (pid == 0) {
			ret = execv(target, t->argv);
			if (ret == -1) {
				fprintf(stderr, "%s: execv: %s\n",
					t->name, strerror(errno));
				abort();
			}
			/* not reached */
		}
		if (waitpid(pid, &status, 0) == -1) {
			fprintf(stderr, "%s: waitpid: %s\n",
				t->name, strerror(errno));
			abort();
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: child didn't exit\n", t->name);
			return 1;
		}
		if (WEXITSTATUS(status)) {
			fprintf(stderr, "%s: exit=%d\n",
				t->name, WEXITSTATUS(status));
			return 1;
		}
	}
	return 0;
}
