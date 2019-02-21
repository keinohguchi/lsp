/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>

static const char *progname;
static const char *const opts = "h";
static const struct option lopts[] = {
	{"help",	no_argument,	NULL,	'h'},
	{NULL,		0,		NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] <path name>\n", progname, opts);
	for (o = lopts; o->name; o++) {
		fprintf(stream, "options\n");
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'h':
			fprintf(stream, "\tdisplay this help and exit\n");
			break;
		}
	}
	exit(status);
}

int main(int argc, char *const argv[])
{
	const char *path;
	int rret, wret;
	int opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	if (optind >= argc)
		usage(stderr, EXIT_FAILURE);

	path = argv[optind];
	if (access(path, F_OK) == 0)
		printf("'%s' exists\n", path);
	else {
		if (errno == ENOENT)
			printf("'%s' does not exit\n", path);
		else if (errno == EACCES)
			printf("'%s' is not accessible\n", path);
		return 1;
	}

	rret = access(path, R_OK);
	if (rret == 0)
		printf("'%s' is readable\n", path);
	else
		printf("'%s' is not readable (permition denied)\n",
		       path);

	wret = access(path, W_OK);
	if (wret == 0)
		printf("'%s' is writable\n", path);
	else if (errno == EACCES)
		printf("'%s' is not writable (permission denied)\n",
		       path);
	else if (errno == EROFS)
		printf("'%s' is not writable (read-only file system)\n",
		       path);

	if (rret || wret)
		return 1;
	return 0;
}
