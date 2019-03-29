/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./netlink", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[10];
		int		want;
	} *t, tests[] = {
		{
			.name	= "-h option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "default(SOCK_RAW|NETLINK_ROUTE) socket",
			.argv	= {target, "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "SOCK_RAW socket",
			.argv	= {target, "-T", "raw", "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "SOCK_DGRAM socket",
			.argv	= {target, "-T", "dgram", "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "SOCK_RAW with NETLINK_ROUTE socket",
			.argv	= {target, "-T", "raw", "-f", "route", "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "SOCK_DGRAM with NETLINK_ROUTE socket",
			.argv	= {target, "-T", "dgram", "-f", "route", "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "edge triggered event with epoll(7)",
			.argv	= {target, "-e", "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "iface option success",
			.argv	= {target, "-i", "lo", "-t", "1", NULL},
			.want	= 0,
		},
		{
			.name	= "iface option failure",
			.argv	= {target, "-i", "nonExistInterface", NULL},
			.want	= 1,
		},
		{ .name = NULL }, /* sentry */
	};
	int ret = 0;

	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		ret = -1;
		pid = fork();
		if (pid == -1) {
			perror("fork");
			break;
		} else if (pid == 0) {
			int ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				exit(EXIT_FAILURE);
			}
			/* not reachable */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			break;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: test got signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			break;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: test did not exit\n",
				t->name);
			break;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected exit status:\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
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
