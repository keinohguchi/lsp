/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
	char *const target = realpath("./sh", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[8];
		const char	*const cmd;
		int		want;
	} *t, tests[] = {
		{
			.name	= "help option",
			.argv	= {target, "-h", NULL},
			.cmd	= NULL,
			.want	= 0,
		},
		{
			.name	= "1 second timeout option",
			.argv	= {target, "-t", "1", NULL},
			.cmd	= NULL,
			.want	= 0,
		},
		{
			.name	= "1 second timeout with prompt option",
			.argv	= {target, "-t", "1", "-p", "someprompt", NULL},
			.cmd	= NULL,
			.want	= 0,
		},
		{
			.name	= "exit command",
			.argv	= {target, NULL},
			.cmd	= "exit\n",
			.want	= 0,
		},
		{
			.name	= "e command",
			.argv	= {target, NULL},
			.cmd	= "e\n",
			.want	= 0,
		},
		{
			.name	= "quit command",
			.argv	= {target, NULL},
			.cmd	= "quit\n",
			.want	= 0,
		},
		{
			.name	= "q command",
			.argv	= {target, NULL},
			.cmd	= "quit\n",
			.want	= 0,
		},
		{
			.name	= "version and exit commands",
			.argv	= {target, NULL},
			.cmd	= "version\nexit\n",
			.want	= 0,
		},
		{
			.name	= "v and exit commands",
			.argv	= {target, NULL},
			.cmd	= "v\nexit\n",
			.want	= 0,
		},
		{
			.name	= "date, uname -an, and exit commands",
			.argv	= {target, NULL},
			.cmd	= "date\nuname -an\nexit\n",
			.want	= 0,
		},
		{
			.name	= "ls -l and exit commands",
			.argv	= {target, NULL},
			.cmd	= "ls -l\nexit\n",
			.want	= 0,
		},
		{
			.name	= "date, uname -an, and exit commands with pipe IPC mode",
			.argv	= {target, "-i", "pipe", NULL},
			.cmd	= "date\nuname -an\nexit\n",
			.want	= 0,
		},
		{
			.name	= "ls -l and exit commands with pipe IPC mode",
			.argv	= {target, "-i", "pipe", NULL},
			.cmd	= "ls -l\nexit\n",
			.want	= 0,
		},
		{
			.name	= "date, uname -an, and exit commands with msgq IPC mode",
			.argv	= {target, "-i", "msgq", NULL},
			.cmd	= "date\nuname -an\nexit\n",
			.want	= 0,
		},
		{
			.name	= "ls -l and exit commands with msgq IPC mode",
			.argv	= {target, "-i", "msgq", NULL},
			.cmd	= "ls -l\nexit\n",
			.want	= 0,
		},
		{ .name = NULL }, /* sentry */
	};
	int ret = 0;

	/* ignore the sigpipe for parent to receive
	 * when child exits */
	signal(SIGPIPE, SIG_IGN);

	for (t = tests; t->name; t++) {
		int fds[2];
		int status;
		pid_t pid;

		/* pipe to pass the command to child */
		ret = pipe(fds);
		if (ret == -1) {
			perror("pipe");
			break;
		}
		ret = -1;
		pid = fork();
		if (pid == -1) {
			perror("fork");
			break;
		} else if (pid == 0) {
			ret = close(fds[1]);
			if (ret == -1) {
				perror("close");
				exit(EXIT_FAILURE);
			}
			ret = dup2(fds[0], STDIN_FILENO);
			if (ret == -1) {
				perror("dup2");
				exit(EXIT_FAILURE);
			}
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				break;
			}
			/* not reach */
		}
		ret = close(fds[0]);
		if (ret == -1) {
			perror("close");
			break;
		}
		if (t->cmd) {
			ret = write(fds[1], t->cmd, strlen(t->cmd));
			if (ret == -1) {
				perror("write");
				break;
			}
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			break;
		}
		ret = close(fds[1]);
		if (ret == -1) {
			perror("close");
			break;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: exit with signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			break;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: did not exit\n", t->name);
			break;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected result\n\t- want: %d\n\t-  got: %d\n",
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
