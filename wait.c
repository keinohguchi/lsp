/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	int status;
	pid_t pid;

	if (!fork()) {
		if (argc > 1) {
			int ret = atoi(argv[1]);
			return ret;
		}
		abort();
	}

	pid = wait(&status);
	if (pid == -1) {
		perror("wait");
		return 1;
	}
	if (WIFEXITED(status))
		printf("Normal exit with exit status=%d\n",
		       WEXITSTATUS(status));
	if (WIFSIGNALED(status))
		printf("Signal received with signal=%d%s\n",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? "(core dumped)" : "");
	return 0;
}
