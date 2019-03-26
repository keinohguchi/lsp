/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>

static struct process {
	const char		*progname;
	int			type;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.type	= SOCK_RAW,
	.opts	= "t:h",
	.lopts	= {
		{"type",	required_argument,	NULL,	't'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0}, /* sentry */
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(s, "\tNetlink socket type [raw|dgram] (default: raw)\n");
			break;
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
	int ret, o;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		switch (o) {
		case 't':
			if (!strncasecmp(optarg, "raw", strlen(optarg)))
				p->type = SOCK_RAW;
			else if (!strncasecmp(optarg, "dgram", strlen(optarg)))
				p->type = SOCK_DGRAM;
			else
				usage(p, stderr, EXIT_FAILURE);
			break;
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = socket(AF_NETLINK, p->type, 0);
	if (ret == -1) {
		perror("socket");
		return 1;
	}
	if (close(ret))
		perror("close");
	return 0;
}
