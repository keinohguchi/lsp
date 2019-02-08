/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/ioctl.h>

/* program context */
static struct context {
	const char		*progname;
	const struct option	lopts[4];
	const char		*const opts;
	struct winsize		win;
	int			all:1;
	int			list:1;
} ctx = {
	.lopts = {
		{"all",		no_argument,	NULL,	'a'},
		{"list",	no_argument,	NULL,	'l'},
		{"help",	no_argument,	NULL,	'h'},
		{},
	},
	.opts	= "alh",
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
			fprintf(stream, "list all files\n");
			break;
		case 'l':
			fprintf(stream, "detailed list\n");
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
	struct dirent *d;
	size_t len, total;
	char *path;
	DIR *dir;
	int ret;

	if ((path = realpath(file, NULL)) == NULL) {
		perror("realpath");
		return -1;
	}
	ret = -1;
	if ((dir = opendir(path)) == NULL) {
		perror("opendir");
		goto out;
	}
	total = 0;
	while ((d = readdir(dir)) != NULL) {
		if (d->d_name[0] == '.' && !ctx->all)
			continue;
		if (ctx->list) {
			printf("%s\n", d->d_name);
			continue;
		}
		len = strlen(d->d_name);
		if (total+len+3 > ctx->win.ws_col) {
			printf("\n");
			total = 0;
		}
		ret = -1;
		len = printf("%s  ", d->d_name);
		if (len < 0) {
			perror("printf");
			goto out;
		}
		total += len;
	}
	if (!ctx->list)
		printf("\n");
	ret = 0;
out:
	if (closedir(dir) == -1) {
		perror("closedir");
		return -1;
	}
	free(path);
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
			ctx.all = 1;
			break;
		case 'l':
			ctx.list = 1;
			break;
		case '?':
		default:
			usage(&ctx, stderr, EXIT_FAILURE);
			break;
		}
	}
	/* get the window size */
	ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ctx.win);
	if (ret == -1 && errno != ENOTTY) {
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
