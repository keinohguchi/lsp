/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>

static const char *progname;

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;

	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t--%s,-%c:\t", o->name, o->val);
		switch (o->val) {
		case 'h':
			fprintf(stream, "show this message\n");
			break;
		default:
			fprintf(stream, "\n");
		}
	}
	exit(status);
}

static int list(const char *const file)
{
	char path[PATH_MAX];

	if (realpath(file, path) == NULL) {
		perror("realpath");
		return -1;
	}
	printf("%s\n", path);
	return 0;
}

int main(int argc, char *const argv[])
{
	const struct option lopts[] = {
		{"help",	no_argument,	NULL,	'h'},
		{},
	};
	const char *const opts = "h";
	int opt, ret;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	if (optind == argc) {
		ret = list(".");
	} else
		while (optind < argc) {
			ret = list(argv[optind++]);
			if (ret == -1)
				return 1;
		}
	return 0;
}
