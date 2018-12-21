/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

/* returns the pointer to the value in len length, or NULL */
char *getattr(const char *path, const char *key, size_t *len)
{
	char *buf = NULL;
	void *val = NULL;
	ssize_t ret;

	buf = malloc(strlen(key)+6);
	if (!buf) {
		perror("malloc");
		return NULL;
	}
	if (sprintf(buf, "user.%s", key) < 0) {
		perror("sprintf");
		goto error;
	}
	ret = getxattr(path, buf, NULL, 0);
	if (ret == -1) {
		perror("getxattr");
		goto error;
	}
	val = malloc(ret);
	if (!val) {
		perror("malloc");
		goto error;
	}
	ret = getxattr(path, buf, val, ret);
	if (ret == -1) {
		perror("getxattr");
		goto error;
	}
	free(buf);
	*len = ret;
	return val;
error:
	free(buf);
	if (val)
		free(val);
	return NULL;
}

int setattr(const char *path, const char *key, const void *val, size_t len)
{
	char *buf;
	int ret;

	buf = malloc(strlen(key)+6);
	if (!buf) {
		perror("malloc");
		return -1;
	}
	ret = -1;
	if (sprintf(buf, "user.%s", key) < 0) {
		perror("sprintf");
		goto out;
	}
	ret = setxattr(path, buf, val, len, 0);
	if (ret == -1) {
		perror("setxattr");
		goto out;
	}
out:
	free(buf);
	return ret;
}

int rmattr(const char *path, const char *key)
{
	char *buf;
	int ret;

	buf = malloc(strlen(key)+6);
	if (!buf) {
		perror("malloc");
		return -1;
	}
	ret = -1;
	if (sprintf(buf, "user.%s", key) < 0) {
		perror("sprintf");
		goto out;
	}
	ret = removexattr(path, buf);
	if (ret == -1) {
		perror("removexattr");
		goto out;
	}
out:
	free(buf);
	return ret;
}
