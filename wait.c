/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

/* let child send signum, if it's non zero and put the
 * result in status */
static int xwait(int ret, int *status)
{
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	}
	if (!pid) {
		/* child */
		if (ret)
			exit(ret);
		abort();
	}
	/* parent */
	return wait(status);
}

int main(int argc, char *argv[])
{
	int ret;
	int status;
	pid_t pid;

	/* dump core in case of no exit status specified */
	if (argc < 2)
		ret = 0;
	else
		ret = atoi(argv[1]);

	/*
	 * fork the child and let it exit with the ret
	 * as the exit status.  The parent, on the other
	 * hand, will wait for it and stores the child
	 * return value in status value, which means
	 * ret and status should be the same.
	 */
	pid = xwait(ret, &status);
	if (pid == -1)
		exit(EXIT_FAILURE);

	if (WIFEXITED(status))
		printf("Normal exit with exit status=%d\n",
		       WEXITSTATUS(status));
	if (WIFSIGNALED(status))
		printf("Signal received with signo=%d%s\n",
		       WTERMSIG(status),
		       WCOREDUMP(status) ? "(core dumped)" : "");

	return 0;
}
