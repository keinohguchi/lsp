/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <sys/wait.h>

static char *const fullpath(const char *const cmd)
{
	static char buf[LINE_MAX];
	size_t len;
	char *endp;

	if (getcwd(buf, sizeof(buf)) == NULL) {
		perror("getcwd");
		abort();
		/* not reached */
		return NULL;
	}
	len = strlen(buf);
	endp = buf+len;
	if (snprintf(endp, sizeof(buf)-len, "/%s", cmd) < 0) {
		perror("snprintf");
		abort();
		/* not reached */
		return NULL;
	}
	return buf;
}

int main()
{
	char *const target = fullpath("ls");
	const struct test {
		const char	*name;
		char *const	argv[5];
		int		want;
	} *t, tests[] = {
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
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
			fprintf(stderr, "unexpected signal: %s\n",
				strsignal(WTERMSIG(status)));
			return 1;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "unexpected non exit\n");
			return 1;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr,
				"unexpected result:\n- want: %d\n-  got: %d\n",
				t->want, WEXITSTATUS(status));
			return 1;
		}
	}
}


