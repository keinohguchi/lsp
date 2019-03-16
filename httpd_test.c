/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./httpd", NULL);
	const struct test {
		char	*name;
		char	*const argv[9];
		int	want;
	} *t, tests[] = {
		{
			.name	= "-h option",
			.argv	= {target, "-h", NULL},
			.want	= 0,
		},
		{
			.name	= "256 listen backlog on port 1024",
			.argv	= {target, "-b", "256", "-p", "1024", NULL},
			.want	= 1,
		},
		{
			.name	= "128 concurrent listener on port 1024",
			.argv	= {target, "-c", "128", "-p", "1024", NULL},
			.want	= 1,
		},
		{
			.name	= "255 listening backlog on port 1024",
			.argv	= {target, "-c", "1", "-b", "255", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "IPv6 255 listening backlog on port 1024",
			.argv	= {target, "-c", "1", "-6", "-b", "255", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "IPv4 on port 1024",
			.argv	= {target, "-c", "1", "-4", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "IPv6 on port 1024",
			.argv	= {target, "-c", "1", "-6", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "IPv4 on port 65534",
			.argv	= {target, "-c", "1", "-4", "-p", "65534", NULL},
			.want	= 0,
		},
		{
			.name	= "IPv6 on port 65534",
			.argv	= {target, "-c", "1", "-6", "-p", "65534", NULL},
			.want	= 0,
		},
		{
			.name	= "two servers 255 listening backlog on port 1024",
			.argv	= {target, "-c", "2", "-b", "255", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "two IPv6 servers 255 listening backlog on port 1024",
			.argv	= {target, "-c", "2", "-6", "-b", "255", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "two IPv4 servers on port 1024",
			.argv	= {target, "-c", "2", "-4", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "two IPv6 servers on port 1024",
			.argv	= {target, "-c", "2", "-6", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "two IPv4 servers on port 65534",
			.argv	= {target, "-c", "2", "-4", "-p", "65534", NULL},
			.want	= 0,
		},
		{
			.name	= "two IPv6 servers on port 65534",
			.argv	= {target, "-c", "2", "-6", "-p", "65534", NULL},
			.want	= 0,
		},
		{
			.name	= "four servers 255 listening backlog on port 1024",
			.argv	= {target, "-c", "4", "-b", "255", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "four IPv6 servers 255 listening backlog on port 1024",
			.argv	= {target, "-c", "4", "-6", "-b", "255", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "four IPv4 servers on port 1024",
			.argv	= {target, "-c", "4", "-4", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "four IPv6 servers on port 1024",
			.argv	= {target, "-c", "4", "-6", "-p", "1024", NULL},
			.want	= 0,
		},
		{
			.name	= "four IPv4 servers on port 65534",
			.argv	= {target, "-c", "4", "-4", "-p", "65534", NULL},
			.want	= 0,
		},
		{
			.name	= "four IPv6 servers on port 65534",
			.argv	= {target, "-c", "4", "-6", "-p", "65534", NULL},
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
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: unexpected signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			break;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: unexpected exit\n", t->name);
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
