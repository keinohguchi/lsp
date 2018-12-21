/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/xattr.h>

char *listattr(const char *path, size_t *len)
{
	ssize_t ret;
	char *buf;

	ret = listxattr(path, NULL, 0);
	if (ret == -1) {
		perror("listxattr");
		return NULL;
	}
	buf = malloc(ret);
	if (!buf) {
		perror("malloc");
		return NULL;
	}
	ret = listxattr(path, buf, ret);
	if (ret == -1) {
		perror("listxattr");
		return NULL;
	}
	*len = ret;
	return buf;
}

int setattr(const char *path, const char *key, const void *buf, size_t len)
{
	int ret = setxattr(path, key, buf, len, 0);
	if (ret == -1)
		perror("setxattr");
	return ret;
}

/* returns the pointer to the value in len length, or NULL */
char *getattr(const char *path, const char *key, size_t *len)
{
	void *buf = NULL;
	ssize_t ret;

	/* check size */
	ret = getxattr(path, key, NULL, 0);
	if (ret == -1) {
		perror("getxattr");
		return NULL;
	}
	buf = malloc(ret);
	if (!buf) {
		perror("malloc");
		goto error;
	}
	ret = getxattr(path, key, buf, ret);
	if (ret == -1) {
		perror("getxattr");
		goto error;
	}
	*len = ret;
	return buf;
error:
	if (buf)
		free(buf);
	return NULL;
}
