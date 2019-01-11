/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <linux/limits.h>

int main(void)
{
	pid_t pid;
	int i;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid)
		/* let parent exit to become daemon */
		exit(EXIT_SUCCESS);

	/* make it session leader/process group leader */
	if (setsid() == -1) {
		perror("setsid");
		return -1;
	}
	if (chdir("/") == -1) {
		perror("chdir");
		return -1;
	}
	/* close all the open files */
	for (i = 0; i < NR_OPEN; i++)
		close(i);
	/* stdin */
	if (open("/dev/null", O_RDWR) == -1) {
		perror("open");
		return -1;
	}
	/* stdout */
	if (dup2(0, 1) == -1) {
		perror("dup2");
		return -1;
	}
	/* stderr */
	if (dup2(0, 2) == -1) {
		perror("dup2");
		return -1;
	}
	for (;;)
		if (pause() == -1)
			perror("pause");
}
