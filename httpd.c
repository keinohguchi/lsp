/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>

static struct context {
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} context = {
	.opts	= "h",
	.lopts	= {
		{"help",	no_argument,	0,	'h'},
		{NULL, 0, NULL, 0}, /* sentry */
	},
};

static void usage(const struct context *restrict ctx, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", ctx->progname, ctx->opts);
	fprintf(s, "options:\n");
	for (o = ctx->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'h':
			fprintf(s, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int init_socket(const struct context *restrict ctx)
{
	return 0;
}

int main(int argc, char *const argv[])
{
	struct context *ctx = &context;
	int ret, opt;

	ctx->progname = argv[0];
	optind = 0;
	while ((opt = getopt_long(argc, argv, ctx->opts, ctx->lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(ctx, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(ctx, stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = init_socket(ctx);
	if (ret == -1)
		return 1;
	return 0;
}
