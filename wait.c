/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* let child send signum, if it's non zero and put the
 * result in status */
int xwait(int ret, int *status)
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
