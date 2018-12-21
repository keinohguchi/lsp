/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *app)
{
	fprintf(stderr,
		"usage: %s <file> [list|get|set] [key [value]]\n",
		app);
	exit(EXIT_SUCCESS);
}

static int list(const char *path)
{
	return 0;
}

static int get(const char *path, const char *key)
{
	return 0;
}

static int set(const char *path, const char *key, const char *val)
{
	return 0;
}

int main(int argc, char *argv[])
{
	const char *path, *cmd;
	int ret = 1;

	if (argc < 3)
		usage(argv[0]);
	path = argv[1];
	cmd = argv[2];
	if (!strncmp(cmd, "list", strlen(cmd)))
		ret = list(path);
	else if (!strncmp(cmd, "get", strlen(cmd))) {
		if (argc < 4)
			usage(argv[0]);
		ret = get(path, argv[3]);
	} else if (!strncmp(cmd, "set", strlen(cmd))) {
		if (argc < 5)
			usage(argv[0]);
		ret = set(path, argv[3], argv[4]);
	} else
		usage(argv[0]);

	return ret;
}
