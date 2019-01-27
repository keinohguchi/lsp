/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/wait.h>

static char *const progpath(const char *const prog)
{
	static char buf[LINE_MAX];
	size_t len;
	char *endp;

	if (getcwd(buf, sizeof(buf)) == NULL) {
		perror("getcwd");
		return NULL;
	}
	len = strlen(buf);
	endp = &buf[len];
	if (snprintf(endp, LINE_MAX-len, "/%s", prog) < 0) {
		perror("snprintf");
		return NULL;
	}
	return buf;
}

int main(void)
{
	const char *const target = "inode";
	char *const targetpath = progpath(target);
	const struct test {
		char	*name;
		char	*const argv[3];
	} *t, tests[] = {
		{
			.name	= "inode itself",
			.argv	= {targetpath, "./inode", NULL},
		},
		{
			.name	= "Makefile",
			.argv	= {targetpath, "Makefile", NULL},
		},
		{
			.name	= "current directory",
			.argv	= {targetpath, ".", NULL},
		},
		{
			.name	= "parent directory",
			.argv	= {targetpath, "..", NULL},
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
			ret = execv(targetpath, t->argv);
			if (ret == -1) {
				perror("execv");
				abort();
			}
			/* cannot reach */
		}
		/* parent */
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			abort();
		}
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s was terminated by signal %s\n",
				target, strsignal(WTERMSIG(status)));
			return 1;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s did not exit\n", target);
			return 1;
		}
		if (WEXITSTATUS(status) != 0) {
			fprintf(stderr, "%s exited with %d\n",
				target, WEXITSTATUS(status));
			return WEXITSTATUS(status);
		}
	}
	return 0;
}
