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
#include <sys/stat.h>

/* ls program context */
static struct context {
	const char		*progname;
	const struct option	lopts[4];
	const char		*const opts;
	struct winsize		win;
	int			all:1;
	int			list:1;
} ls = {
	.lopts = {
		{"all",		no_argument,	NULL,	'a'},
		{"list",	no_argument,	NULL,	'l'},
		{"help",	no_argument,	NULL,	'h'},
		{},
	},
	.opts	= "alh",
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", ls.progname, ls.opts);
	fprintf(stream, "options:\n");
	for (o = ls.lopts; o->name; o++) {
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

static size_t print_file(const char *const file)
{
	if (!ls.list)
		return printf("%s  ", file);
	return printf("%s\n", file);
}

static int list(const char *const file)
{
	struct stat st;
	struct dirent *dp;
	size_t len, total;
	DIR *dir = NULL;
	char *path;
	int ret;

	if ((path = realpath(file, NULL)) == NULL) {
		perror("realpath");
		return -1;
	}
	ret = lstat(path, &st);
	if (ret == -1) {
		perror("stat");
		goto out;
	}
	ret = 0;
	if (!S_ISDIR(st.st_mode)) {
		len = print_file(file);
		if (len < 0)
			ret = 1;
		else if (!ls.list)
			printf("\n");
		goto out;
	}
	ret = -1;
	if ((dir = opendir(path)) == NULL) {
		perror("opendir");
		goto out;
	}
	total = 0;
	while ((dp = readdir(dir)) != NULL) {
		if (dp->d_name[0] == '.' && !ls.all)
			continue;
		if (ls.list) {
			printf("%s\n", dp->d_name);
			continue;
		}
		len = strlen(dp->d_name);
		if (total+len+3 > ls.win.ws_col) {
			printf("\n");
			total = 0;
		}
		ret = -1;
		len = print_file(dp->d_name);
		if (len < 0) {
			perror("printf");
			goto out;
		}
		total += len;
	}
	if (!ls.list)
		printf("\n");
	ret = 0;
out:
	if (dir)
		if (closedir(dir) == -1) {
			perror("closedir");
			ret = -1;
		}
	if (path)
		free(path);
	return ret;
}

int main(int argc, char *const argv[])
{
	int opt, ret;

	ls.progname = argv[0];
	while ((opt = getopt_long(argc, argv, ls.opts, ls.lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case 'a':
			ls.all = 1;
			break;
		case 'l':
			ls.list = 1;
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	/* get the window size */
	ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ls.win);
	if (ret == -1 && errno != ENOTTY) {
		perror("ioctl");
		goto out;
	}
	/* let's rock */
	if (optind == argc)
		ret = list(".");
	else
		while (optind < argc)
			if ((ret = list(argv[optind++])) == -1)
				goto out;
out:
	if (ret != 0)
		return 1;
	return 0;
}
