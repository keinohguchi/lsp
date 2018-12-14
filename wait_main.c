/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(int argc, char *argv[])
{
	extern pid_t xwait(int ret, int *status);
	int ret = 0;
	int status;
	pid_t pid;

	if (argc > 1)
		ret = atoi(argv[1]);

	pid = xwait(ret, &status);
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
