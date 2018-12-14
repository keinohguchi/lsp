/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

extern pid_t _wait(int ret, int *status);

int main(int argc, char *argv[])
{
	int ret = 0;
	int status;
	pid_t pid;

	if (argc > 1)
		ret = atoi(argv[1]);

	pid = _wait(ret, &status);
	if (pid == -1)
		return 1;

	if (WIFEXITED(status))
		printf("Normal exit with exit status=%d\n",
		       WEXITSTATUS(status));
	if (WIFSIGNALED(status))
		printf("Signal received with signal=%d%s\n",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? "(core dumped)" : "");
	return 0;
}
