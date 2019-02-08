/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

static const char *progname;

struct options {
	bool all;
};

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;

	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t--%s,-%c:\t", o->name, o->val);
		switch (o->val) {
		case 'a':
			fprintf(stream, "show all files\n");
			break;
		case 'h':
			fprintf(stream, "show this message\n");
			break;
		default:
			fprintf(stream, "%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int list(const char *const file, const struct options *const o)
{
	char path[PATH_MAX];
	struct dirent *d;
	DIR *dir;
	int ret;

	if (realpath(file, path) == NULL) {
		perror("realpath");
		return -1;
	}
	if ((dir = opendir(path)) == NULL) {
		perror("opendir");
		return -1;
	}
	while ((d = readdir(dir)) != NULL) {
		if (d->d_name[0] == '.' && !o->all)
			continue;
		printf("%s\n", d->d_name);
	}
	ret = 0;
	if (closedir(dir) == -1) {
		perror("closedir");
		return -1;
	}
	return ret;
}

int main(int argc, char *const argv[])
{
	const struct option lopts[] = {
		{"all",		no_argument,	NULL,	'a'},
		{"help",	no_argument,	NULL,	'h'},
		{},
	};
	const char *const opts = "ah";
	struct options o;
	int opt, ret;

	progname = argv[0];
	memset(&o, 0, sizeof(o));
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case 'a':
			o.all = true;
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	/* let's rock */
	if (optind == argc)
		ret = list(".", &o);
	else
		while (optind < argc)
			if ((ret = list(argv[optind++], &o)) == -1)
				goto done;
done:
	if (ret != 0)
		return 1;
	return 0;
}
