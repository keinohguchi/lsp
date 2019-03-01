/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
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
static const char *opts = "n:h";
static const struct option lopts[] = {
	{"name",	required_argument,	NULL,	'n'},
	{"help",	no_argument,		NULL,	'h'},
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
			fprintf(stream, "\tfind specified name\n");
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

static int find(const char *restrict path, const char *file, const char *restrict pattern)
{
	char buf[PATH_MAX];
	struct dirent *d;
	struct stat s;
	DIR *dir;
	int ret;

	if (!fnmatch(pattern, file, FNM_EXTMATCH))
		printf("%s\n", path);

	ret = lstat(path, &s);
	if (ret == -1) {
		perror("stat");
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
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		pathname(path, d->d_name, buf, sizeof(buf));
		if ((ret = find(buf, d->d_name, pattern)))
			break;
	}
	if (closedir(dir))
		perror("closedir");
	return ret;
}

int main(int argc, char *argv[])
{
	const char *pattern = "*";
	int opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1)
		switch (opt) {
		case 'n':
			pattern = optarg;
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

	if (find(argv[optind], argv[optind], pattern))
		return 1;
	return 0;
}
