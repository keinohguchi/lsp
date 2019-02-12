/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <string.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/stat.h>

/* ls program context */
static struct context {
	const char		*progname;
	struct winsize		win;
	int			all:1;
	int			list:1;
	const char		*const opts;
	const struct option	lopts[];
} ls = {
	.opts	= "alh",
	.lopts	= {
		{"all",		no_argument,	NULL,	'a'},
		{"list",	no_argument,	NULL,	'l'},
		{"help",	no_argument,	NULL,	'h'},
		{},
	},
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", ls.progname, ls.opts);
	fprintf(stream, "options:\n");
	for (o = ls.lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:\t", o->val, o->name);
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

static size_t print_file_long(const char *const file, struct stat *restrict st)
{
	size_t len, total = 0;
	struct passwd *pwd;
	struct group *grp;
	char timebuf[BUFSIZ];
	struct tm tm;

	if (st == NULL) {
		struct stat sbuf;
		if (lstat(file, &sbuf) == -1) {
			perror("lstat");
			return -1;
		}
		st = &sbuf;
	}
	if ((len = printf("%2ld", st->st_nlink)) < 0) {
		perror("printf");
		return -1;
	}
	total += len;
	if ((pwd = getpwuid(st->st_uid)) != NULL) {
		if ((len = printf(" %-s", pwd->pw_name)) < 0) {
			perror("printf");
			return -1;
		}
	} else {
		if ((len = printf(" %-d", st->st_uid)) < 0) {
			perror("printf");
			return -1;
		}
	}
	total += len;
	if ((grp = getgrgid(st->st_gid)) != NULL) {
		if ((len = printf(" %-s", grp->gr_name)) < 0) {
			perror("printf");
			return -1;
		}
	} else {
		if ((len = printf(" %-d", st->st_gid)) < 0) {
			perror("printf");
			return -1;
		}
	}
	if ((len = printf(" %7jd", st->st_size)) < 0) {
		perror("printf");
		return -1;
	}
	total += len;
	if (localtime_r(&st->st_mtime, &tm) == NULL) {
		perror("localtie_r");
		return -1;
	}
	if (strftime(timebuf, sizeof(timebuf), "%b %d %k:%M", &tm) < 0) {
		perror("strftime");
		return -1;
	}
	if ((len = printf(" %-s %s\n", timebuf, file)) < 0) {
		perror("printf");
		return -1;
	}
	return total += len;
}

static size_t print_file(const char *const file, struct stat *restrict st)
{
	if (ls.list)
		return print_file_long(file, st);
	else
		return printf("%s%s", file, ls.win.ws_col ? "  " : "\n");
}

static int list(const char *const file)
{
	struct stat st;
	struct dirent *d;
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
	if (!S_ISDIR(st.st_mode)) {
		ret = -1;
		if (print_file(file, &st) < 0)
			goto out;
		if (!ls.list)
			if (printf("\n") < 0)
				goto out;
		ret = 0;
		goto out;
	}
	ret = -1;
	if ((dir = opendir(path)) == NULL) {
		perror("opendir");
		goto out;
	}
	total = 0;
	while ((d = readdir(dir)) != NULL) {
		if (d->d_name[0] == '.' && !ls.all)
			continue;
		ret = -1;
		if ((len = print_file(d->d_name, NULL)) < 0)
			goto out;
		if (ls.list)
			continue;
		total += len;
		if (ls.win.ws_col && total+1 >= ls.win.ws_col) {
			if (printf("\n") < 0)
				goto out;
			total = 0;
		}
	}
	ret = -1;
	if (!ls.list && ls.win.ws_col)
		if (printf("\n") < 0)
			goto out;
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
	ls.win.ws_col = 0;
	if (isatty(STDOUT_FILENO)) {
		ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ls.win);
		if (ret == -1) {
			perror("ioctl");
			goto out;
		}
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
