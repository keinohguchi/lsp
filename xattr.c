/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/xattr.h>

static const char *progname;

static char *lsattr(const char *path, size_t *len)
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
static char *getattr(const char *path, const char *key, size_t *len)
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

static int setattr(const char *path, const char *key, const void *val, size_t len)
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

static int rmattr(const char *path, const char *key)
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

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] path [key [value]]\n", progname, opts);
	fprintf(stream, "options\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s", o->val, o->name);
		switch (o->val) {
		case 'l':
			fprintf(stream, "\tlist extra attributes\n");
			break;
		case 'r':
			fprintf(stream, "\t\tremove extra attribute\n");
			break;
		case 'h':
			fprintf(stream, "\tshow this message\n");
			break;
		case 'g':
		case 's':
		default:
			fprintf(stream, "\t%s extra attribute\n", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	const char *opts = "lgsrh";
	struct option lopts[] = {
		{"list",	no_argument,	NULL, 'l'},
		{"get",		no_argument,	NULL, 'g'},
		{"set",		no_argument,	NULL, 's'},
		{"rm",		no_argument,	NULL, 'r'},
		{"help",	no_argument,	NULL, 'h'},
		{}, /* sentry */
	};
	const char *path, *key, *value;
	enum cmd {
		CMDGET = 0,
		CMDSET,
		CMDRM,
		CMDLIST,
		CMDMAX,
	} cmds[CMDMAX];
	int opt, ret, i;

	progname = argv[0];
	memset(cmds, 0, sizeof(cmds));
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'l':
			cmds[CMDLIST] = 1;
			break;
		case 'g':
			cmds[CMDGET] = 1;
			break;
		case 's':
			cmds[CMDSET] = 1;
			break;
		case 'r':
			cmds[CMDRM] = 1;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	if (optind >= argc)
		usage(stderr, EXIT_FAILURE, opts, lopts);
	path = argv[optind++];

	key = value = NULL;
	ret = 0;
	for (i = 0; i < CMDMAX; i++) {
		char *retp, *ptr, *endp;
		size_t len = 0;
		switch (i) {
		case CMDLIST:
			/* always list the attribute */
			retp = lsattr(path, &len);
			if (retp == NULL) {
				fprintf(stderr, "cannot get the attribute list\n");
				ret = 1;
				goto out;
			}
			printf("%s: list\n", path);
			for (ptr = retp, endp = ptr+len; ptr < endp; ptr += len) {
				printf("\t");
				len = printf("%s\n", ptr);
				if (len < 0) {
					perror("printf");
					break;
				}
			}
			free(retp);
			break;
		case CMDGET:
			if (!cmds[i])
				continue;
			if (!key && optind >= argc)
				usage(stderr, EXIT_FAILURE, opts, lopts);
			key = argv[optind++];
			retp = getattr(path, key, &len);
			if (retp == NULL) {
				fprintf(stderr, "%s: cannot get the value for %s\n",
					path, key);
				ret = 1;
				goto out;
			}
			retp[len] = '\0';
			printf("%s: %s=%s\n", path, key, retp);
			free(retp);
			break;
		case CMDSET:
			if (!cmds[i])
				continue;
			if (!key) {
				if (optind >= argc)
					usage(stderr, EXIT_FAILURE, opts, lopts);
				key = argv[optind++];
			}
			if (!value) {
				if (optind >= argc)
					usage(stderr, EXIT_FAILURE, opts, lopts);
				value = argv[optind++];
			}
			len = strlen(value)+1;
			ret = setattr(path, key, value, len);
			if (ret == -1) {
				fprintf(stderr, "%s: cannot set attribute %s=%s\n",
					path, key, value);
				ret = 1;
				goto out;
			}
			printf("%s: %s=%s\n", path, key, value);
			break;
		case CMDRM:
			if (!cmds[i])
				continue;
			if (!key) {
				if (optind >= argc)
					usage(stderr, EXIT_FAILURE, opts, lopts);
				key = argv[optind++];
			}
			ret = rmattr(path, key);
			if (ret == -1) {
				fprintf(stderr, "%s: cannot remove attribute %s\n",
					path, key);
				ret = 1;
				goto out;
			}
			printf("%s: %s removed\n", path, key);
			break;
		default:
			fprintf(stderr, "unsupported command\n");
			ret = 1;
			goto out;
			break;
		}
	}
out:
	return ret;
}
