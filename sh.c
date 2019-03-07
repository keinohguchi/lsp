/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>

static const char *progname;
static const char *const opts = "p:h";
static const struct option lopts[] = {
	{"prompt",	required_argument,	NULL,	'p'},
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
		case 'p':
			fprintf(stream, "\tspecify the prompt\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *const argv[])
{
	const char *prompt = "sh";
	char *cmd, line[LINE_MAX];
	int opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'p':
			prompt = optarg;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	printf("%s$ ", prompt);
	while ((cmd = fgets(line, sizeof(line), stdin)))
		printf("%s$ ", prompt);
	printf("\n");
	return 0;
}
