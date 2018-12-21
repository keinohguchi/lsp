/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *listattr(const char *path, size_t *len);
extern int setattr(const char *path, const char *key, const void *buf, size_t len);
extern char *getattr(const char *path, const char *key, size_t *len);

int main(int argc, char *argv[])
{
	const char *key = "user:something";
	char *val = "something";
	char *keys, *ptr, *endp;
	size_t len;
	int ret;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <file> [<key> [<value>]]\n", argv[0]);
		return 0;
	}
	if (argc > 2)
		key = argv[2];
	if (argc > 3) {
		val = argv[3];
		len = strlen(val)+1;
	}
	keys = listattr(argv[1], &len);
	if (!keys) {
		printf("%s: cannot get the attribute list\n", argv[1]);
		return 1;
	}
	for (ptr = val, endp = ptr+len; ptr != endp; ptr += len) {
		len = printf("%s\n", ptr);
		if (len < 0) {
			perror("printf");
			free(keys);
			return 1;
		}
	}
	ret = setattr(argv[1], key, val, len);
	if (ret == -1) {
		printf("%s: cannot set value=%s for key=%s\n", argv[1], val, key);
		return 1;
	}
	val = getattr(argv[1], key, &len);
	if (!val) {
		printf("%s: no value for key=%s\n", argv[1], key);
		return 1;
	}
	printf("%s=%s\n", key, val);
	free(val);
	return 0;
}
