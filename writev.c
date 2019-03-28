/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

static struct process {
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.opts	= "h",
	.lopts	= {
		{"help",	no_argument,	NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *const p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s] <file>\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'h':
			fprintf(s, "\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	int o, ret, fd;
	char *path;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		switch (o) {
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	}
	if (optind >= argc)
		usage(p, stderr, EXIT_FAILURE);
	path = argv[optind];

	ret = -1;
	fd = open(path, O_WRONLY|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	ret = 0;
	if (close(fd))
		perror("close");
	if (ret)
		return 1;
	return 0;
}
