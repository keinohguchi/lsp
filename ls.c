/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* program context */
static struct context {
	const char		*progname;
	const struct option	lopts[3];
	const char		*const opts;
	struct winsize		win;
	bool			all;
} ctx = {
	.lopts = {
		{"all",		no_argument,	NULL,	'a'},
		{"help",	no_argument,	NULL,	'h'},
		{},
	},
	.opts	= "ah",
};

static void usage(const struct context *const ctx, FILE *stream, int status)
{
	const struct option *o;

	fprintf(stream, "usage: %s [-%s]\n", ctx->progname, ctx->opts);
	fprintf(stream, "options:\n");
	for (o = ctx->lopts; o->name; o++) {
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

static int list(const struct context *const ctx, const char *const file)
{
	char path[PATH_MAX];
	struct dirent *d;
	size_t len, total;
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
	total = 0;
	while ((d = readdir(dir)) != NULL) {
		if (d->d_name[0] == '.' && !ctx->all)
			continue;
		if (total + strlen(d->d_name) + 2 > ctx->win.ws_col-1) {
			printf("\n");
			total = 0;
		}
		len = printf("%s  ", d->d_name);
		if (len < 0) {
			perror("printf");
			ret = -1;
			goto out;
		}
		total += len;
	}
	printf("\n");
	ret = 0;
out:
	if (closedir(dir) == -1) {
		perror("closedir");
		return -1;
	}
	return ret;
}

int main(int argc, char *const argv[])
{
	int opt, ret;

	ctx.progname = argv[0];
	while ((opt = getopt_long(argc, argv, ctx.opts, ctx.lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(&ctx, stdout, EXIT_SUCCESS);
			break;
		case 'a':
			ctx.all = true;
			break;
		case '?':
		default:
			usage(&ctx, stderr, EXIT_FAILURE);
			break;
		}
	}
	/* get the window size */
	ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ctx.win);
	if (ret == -1) {
		perror("ioctl");
		goto out;
	}
	/* let's rock */
	if (optind == argc)
		ret = list(&ctx, ".");
	else
		while (optind < argc)
			if ((ret = list(&ctx, argv[optind++])) == -1)
				goto out;
out:
	if (ret != 0)
		return 1;
	return 0;
}
