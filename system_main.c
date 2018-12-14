/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>

extern int _system(char *const argv[]);

int main(int argc, char *argv[])
{
	char **args;
	int ret;
	int i;

	if (argc < 2) {
		printf("usage: %s <command>\n", argv[0]);
		return 0;
	}
	/* allocate the memory to host the new command */
	args = malloc(sizeof(char *)*(argc+2));
	if (args == NULL) {
		perror("malloc");
		return -1;
	}
	/* create an argument for execv(2) */
	args[0] = "sh";
	args[1] = "-c";
	for (i = 0; i < argc-1; i++)
		args[i+2] = argv[i+1];
	args[i+2] = NULL;
	ret = _system(args);
out:
	free(args);
	return ret;
}
