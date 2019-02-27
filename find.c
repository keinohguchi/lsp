/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

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
	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
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

int main(int argc, char *argv[])
{
	const char *pattern = NULL;
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
	printf("find '%s'\n", pattern);
	return 0;
}
