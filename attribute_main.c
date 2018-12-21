/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *lsattr(const char *path, size_t *len);
extern char *getattr(const char *path, const char *key, size_t *len);
extern int setattr(const char *path, const char *key, const char *val, size_t len);
extern int rmattr(const char *path, const char *key);

static void usage(const char *app)
{
	fprintf(stderr,
		"usage: %s [ls|get|set|rm] <file> [<key> [<value>]]\n",
		app);
	exit(EXIT_SUCCESS);
}

static int ls(const char *path)
{
	char *attrs, *ptr, *endp;
	size_t len;
	int ret = 0;

	attrs = lsattr(path, &len);
	if (!attrs)
		return 1;
	ret = 1;
	for (ptr = attrs, endp = ptr+len; ptr < endp; ptr += len) {
		len = printf("%s\n", ptr);
		if (len < 0) {
			perror("printf");
			break;
		}
	}
	free(attrs);
	return ret;
}

static int get(const char *path, const char *key)
{
	size_t len;
	char *val;

	val = getattr(path, key, &len);
	if (!val)
		return 1;
	printf("%s=%s\n", key, val);
	free(val);
	return 0;
}

static int set(const char *path, const char *key, const char *val)
{
	int ret;

	ret = setattr(path, key, val, strlen(val)+1);
	if (ret == -1)
		return 1;
	return 0;
}

static int rm(const char *path, const char *key)
{
	int ret;

	ret = rmattr(path, key);
	if (ret == -1)
		return 1;
	return 0;
}

int main(int argc, char *argv[])
{
	const char *path, *cmd;
	int ret = 1;

	if (argc < 3)
		usage(argv[0]);
	cmd = argv[1];
	path = argv[2];
	if (!strncmp(cmd, "ls", strlen(cmd)))
		ret = ls(path);
	else if (!strncmp(cmd, "get", strlen(cmd))) {
		if (argc < 4)
			usage(argv[0]);
		ret = get(path, argv[3]);
	} else if (!strncmp(cmd, "set", strlen(cmd))) {
		if (argc < 5)
			usage(argv[0]);
		ret = set(path, argv[3], argv[4]);
	} else if (!strncmp(cmd, "rm", strlen(cmd))) {
		if (argc < 4)
			usage(argv[0]);
		ret = rm(path, argv[3]);
	} else
		usage(argv[0]);

	return ret;
}
