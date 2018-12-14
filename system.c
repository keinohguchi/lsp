/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int xsystem(char *const argv[])
{
	int status;
	pid_t pid;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid == 0) {
		/* child */
		if (execv("/bin/sh", argv) == -1) {
			perror("execv");
			exit(EXIT_FAILURE);
		}
		exit(EXIT_SUCCESS);
	}
	/* parent */
	if (waitpid(pid, &status, 0) == -1) {
		perror("waitpid");
		return -1;
	} else if (WIFEXITED(status))
		return WEXITSTATUS(status);
	else
		return -1;
}
