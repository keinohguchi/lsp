/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <sys/socket.h>

static const char *progname;
static struct parameter {
	int	timeout;
	int	family;
	int	type;
	int	proto;
	int	port;
} param = {
	.family		= PF_INET,
	.type		= SOCK_STREAM,
	.proto		= 0,
	.port		= 9999,
};
static const char *const opts = "h";
static const struct option lopts[] = {
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
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	struct parameter *p = &param;
	int opt;
	int s;

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
	s = socket(p->family, p->type, p->proto);
	if (s == -1) {
		perror("socket");
		return 1;
	}
	if (close(s)) {
		perror("close");
		return 1;
	}
	return 0;
}
