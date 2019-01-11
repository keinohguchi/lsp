/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* my own system command */
static int xsystem(char *const argv[])
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

int main(int argc, char *argv[])
{
	char **args;
	int ret;
	int i;

	if (argc < 2) {
		printf("usage: %s <command>\n", argv[0]);
		exit(EXIT_SUCCESS);
	}
	/* allocate memory for the new command */
	args = calloc((argc+2), sizeof(char *));
	if (!args) {
		perror("calloc");
		exit(EXIT_FAILURE);
	}
	/* for execv(2) */
	args[0] = "sh";
	args[1] = "-c";
	for (i = 0; i < argc-1; i++)
		args[i+2] = argv[i+1];
	args[i+2] = NULL;
	ret = xsystem(args);
	free(args);
	return ret;
}
