/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

const char *const progname = "thread";

static char *const progpath(const char *const progname)
{
	static char buf[LINE_MAX];
	size_t len;
	char *endp;

	if (getcwd(buf, sizeof(buf)) == NULL) {
		perror("getcwd");
		return NULL;
	}
	len = strlen(buf);
	endp = buf+len;
	if (snprintf(endp, LINE_MAX-len, "/%s", progname) < 0) {
		perror("snprintf");
		return NULL;
	}
	return buf;
}

int main(void)
{
	char *const path = progpath(progname);
	const struct test {
		const char	*name;
		char		*const argv[5];
	} *t, tests[] = {
		{
			.name	= "1 thread",
			.argv	= {path, "-c", "1", NULL},
		},
		{
			.name	= "2 threads",
			.argv	= {path, "-c", "2", NULL},
		},
		{
			.name	= "12 threads",
			.argv	= {path, "-c", "12", NULL},
		},
		{
			.name	= "512 threads",
			.argv	= {path, "-c", "512", NULL},
		},
		{
			.name	= "1024 threads",
			.argv	= {path, "-c", "1024", NULL},
		},
		{}, /* sentry */
	};

	for (t = tests; t->name; t++) {
		int ret, status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s: fork: %s\n",
				t->name, strerror(errno));
			abort();
		} else if (pid == 0) {
			/* child */
			ret = execv(path, t->argv);
			if (ret == -1) {
				fprintf(stderr, "%s: execv: %s\n",
					t->name, strerror(errno));
				abort();
			}
			/* not reached */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			fprintf(stderr, "%s: waitpid: %s\n",
				t->name, strerror(errno));
			abort();
		}
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: got signal %s\n",
				t->name, strsignal(WTERMSIG(status)));
			abort();
		}
		if (WIFEXITED(status)) {
			if (WEXITSTATUS(status) != 0) {
				fprintf(stderr, "%s: got exit status %d\n",
					t->name, WEXITSTATUS(status));
				abort();
			}
		}
	}
	return 0;
}
