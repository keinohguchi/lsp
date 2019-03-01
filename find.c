/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <fnmatch.h>
#include <sys/stat.h>

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

static int find(const char *restrict path, const char *restrict pattern)
{
	struct stat s;
	int ret;

	if (!fnmatch(pattern, path, FNM_EXTMATCH))
		printf("%s\n", path);

	ret = lstat(path, &s);
	if (ret == -1) {
		perror("lstat");
		return ret;
	}
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

	if (find(argv[optind], pattern))
		return 1;
	return 0;
}
