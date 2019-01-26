/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>

static const char *prog = "resource";

static char *const targetpath(const char *prog)
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
		char		*const argv[5];
	} tests[] = {
		{
			.name	= "-h option",
			.argv	= {target, "-h", NULL},
		},
		{
			.name	= "--help option",
			.argv	= {target, "--help", NULL},
		},
		{
			.name	= "-a option",
			.argv	= {target, "-a", NULL},
		},
		{
			.name	= "--all option",
			.argv	= {target, "--all", NULL},
		},
		{
			.name	= "-c option",
			.argv	= {target, "-c", NULL},
		},
		{
			.name	= "--core option",
			.argv	= {target, "--core", NULL},
		},
		{
			.name	= "-d option",
			.argv	= {target, "-d", NULL},
		},
		{
			.name	= "--data option",
			.argv	= {target, "--data", NULL},
		},
		{
			.name	= "-f option",
			.argv	= {target, "-f", NULL},
		},
		{
			.name	= "--file option",
			.argv	= {target, "--file", NULL},
		},
		{
			.name	= "-o option",
			.argv	= {target, "-o", NULL},
		},
		{
			.name	= "--open option",
			.argv	= {target, "--open", NULL},
		},
		{
			.name	= "-p option",
			.argv	= {target, "-p", NULL},
		},
		{
			.name	= "--cpu option",
			.argv	= {target, "--cpu", NULL},
		},
		{
			.name	= "--file and --cpu options",
			.argv	= {target, "--file", "--cpu", NULL},
		},
		{}, /* sentry */
	};
	const struct test *t;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		pid = fork();
		if (pid == -1) {
			fprintf(stderr, "%s: fork: %s\n",
				t->name, strerror(errno));
			abort();
		}
		if (pid == 0) {
			if (execv(target, t->argv) == -1) {
				fprintf(stderr, "%s: execv: %s\n",
					t->name, strerror(errno));
				abort();
			}
			/* not reachable */
		}
		if (waitpid(pid, &status, 0) == -1) {
			fprintf(stderr, "%s: waitpid: %s\n",
				t->name, strerror(errno));
			abort();
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: does not exit\n",
				t->name);
			return 1;
		}
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: killed by %s\n",
				t->name, strsignal(WTERMSIG(status)));
			return 1;
		}
		if (WEXITSTATUS(status) != 0) {
			fprintf(stderr, "%s: returns %d\n",
				t->name, WEXITSTATUS(status));
			return WEXITSTATUS(status);
		}
	}
	return 0;
}
