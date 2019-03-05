/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <fnmatch.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *progname;
static const char *pattern;
static bool recursive = true;
static const char *opts = "n:h";
static const struct option lopts[] = {
	{"name",	required_argument,	NULL,	'n'},
	{"help",	no_argument,		NULL,	'h'},
	{"recursive",	no_argument,		NULL,	'r'},
	{NULL,		0,			NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] <directory name>\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'n':
			fprintf(stream, "\tfind specified pattern\n");
			break;
		case 'r':
			fprintf(stream, "\trecursively find the file\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	_exit(status);
}

static const char *pathname(const char *base, const char *file, char *buf, size_t len)
{
	switch (strlen(base)) {
	case 0:
		return file;
	default:
		if (base[strlen(base)-1] == '/') {
			if (snprintf(buf, len, "%s%s", base, file) < 0)
				return file;
		} else {
			if (snprintf(buf, len, "%s/%s", base, file) < 0)
				return file;
		}
		return buf;
	}
}

static int pathmatch(const char *restrict path, const char *restrict pattern)
{
	char *tmp, *name;
	int ret;
	tmp = strdup(path);
	if (tmp == NULL) {
		perror("strdup");
		return -1;
	}
	name = basename(tmp);
	ret = fnmatch(pattern, name, FNM_EXTMATCH);
	free(tmp);
	return ret;
}

static int find(const char *const path)
{
	struct dirent *d;
	struct stat s;
	DIR *dir;
	int ret;

	if (!pathmatch(path, pattern))
		printf("%s\n", path);

	ret = lstat(path, &s);
	if (ret == -1) {
		perror("lstat");
		return -1;
	}
	if (!S_ISDIR(s.st_mode))
		return 0;

	dir = opendir(path);
	if (dir == NULL) {
		/* ignore this directory */
		perror("opendir");
		return 0;
	}
	while ((d = readdir(dir))) {
		char *child;
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		child = malloc(PATH_MAX);
		if (child == NULL) {
			perror("malloc");
			ret = -1;
			break;
		}
		pathname(path, d->d_name, child, PATH_MAX);
		if (recursive)
			ret = find(child);
		else
			/* thread version will come here */
			ret = 0;
		free(child);
		if (ret)
			break;
	}
	if (closedir(dir))
		perror("closedir");
	return ret;
}

int main(int argc, char *argv[])
{
	const char *path;
	int opt;

	progname = argv[0];
	pattern = "*";
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1)
		switch (opt) {
		case 'n':
			pattern = optarg;
			break;
		case 'r':
			recursive = true;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	if (optind >= argc)
		usage(stderr, EXIT_FAILURE);

	/* let's roll */
	path = argv[optind];
	if (find(path))
		return 1;
	return 0;
}
