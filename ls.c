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
#include <sys/stat.h>
#include <sys/ioctl.h>

/* ls program context */
static int version_flag = 0;
static int help_flag = 0;
static struct context {
	const char		*progname;
	struct winsize		win;
	int			all:1;
	int			list:1;
	const char		*const version;
	const char		*const opts;
	const struct option	lopts[];
} ls = {
	.version	= "1.0.0",
	.opts		= "al",
	.lopts		= {
			{"all",		no_argument,	NULL,		'a'},
			{"list",	no_argument,	NULL,		'l'},
			{"version",	no_argument,	&version_flag,	1},
			{"help",	no_argument,	&help_flag,	1},
			{},
	},
};

static void version(FILE *stream)
{
	fprintf(stream, "%s version %s\n", ls.progname, ls.version);
	exit(EXIT_SUCCESS);
}

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", ls.progname, ls.opts);
	fprintf(stream, "options:\n");
	for (o = ls.lopts; o->name; o++) {
		fprintf(stream, "\t");
		if (!o->flag)
			fprintf(stream, "-%c,", o->val);
		fprintf(stream, "--%s:\t", o->name);
		switch (o->val) {
		case 'a':
			fprintf(stream, "list all files\n");
			break;
		case 'l':
			fprintf(stream, "detailed list\n");
			break;
		case 1:
			switch (o->name[0]) {
			case 'v':
				fprintf(stream, "show version information\n");
				continue;
			case 'h':
				fprintf(stream, "\tshow this message\n");
				continue;
			}
		default:
			fprintf(stream, "%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static char *stmode(mode_t mode, char *restrict buf, size_t size)
{
	const char *const rwx[] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
	const char *u, *g, *o;
	char type;
	switch (mode&S_IFMT) {
	case S_IFBLK:	type = 'b'; break;
	case S_IFCHR:	type = 'c'; break;
	case S_IFDIR:	type = 'd'; break;
	case S_IFIFO:	type = 'p'; break;
	case S_IFLNK:	type = 'l'; break;
	case S_IFSOCK:	type = 's'; break;
	default:	type = '-'; break;
	}
	u = rwx[mode>>6&7];
	g = rwx[mode>>3&7];
	o = rwx[mode&7];
	if (snprintf(buf, size, "%c%3s%3s%3s", type, u, g, o) < 0) {
		perror("snprintf");
		return NULL;
	}
	/* sticky bits */
	if (mode&S_ISUID)
		buf[3] = mode&S_IXUSR ? 's' : 'S';
	if (mode&S_ISGID)
		buf[6] = mode&S_IXGRP ? 's' : 'S';
	if (mode&S_ISVTX)
		buf[9] = mode&S_IXOTH ? 't' : 'T';
	return buf;
}

static size_t print_file_long(const char *const file, struct stat *restrict st)
{
	size_t len, total = 0;
	struct passwd *pwd;
	struct group *grp;
	char buf[BUFSIZ];
	struct tm tm;

	if (st == NULL) {
		struct stat sbuf;
		if (lstat(file, &sbuf) == -1) {
			perror("lstat");
			return -1;
		}
		st = &sbuf;
	}
	if (stmode(st->st_mode, buf, sizeof(buf)) == NULL)
	    return -1;
	if ((len = printf("%s %ld", buf, st->st_nlink)) < 0) {
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
	if (strftime(buf, sizeof(buf), "%b %d %k:%M", &tm) < 0) {
		perror("strftime");
		return -1;
	}
	if ((len = printf(" %-s %s\n", buf, file)) < 0) {
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
		if (ls.win.ws_col && total+strlen(d->d_name)+3 >= ls.win.ws_col) {
			if (printf("\n") < 0)
				goto out;
			total = 0;
		}
		ret = -1;
		if ((len = print_file(d->d_name, NULL)) < 0)
			goto out;
		if (ls.list)
			continue;
		total += len;
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
	int i, opt, ret;

	ls.progname = argv[0];
	while ((opt = getopt_long(argc, argv, ls.opts, ls.lopts, &i)) != -1) {
		switch (opt) {
		case 0:
			switch (ls.lopts[i].name[0]) {
			case 'v':
				version(stdout);
				break;
			case 'h':
				usage(stdout, EXIT_SUCCESS);
				break;
			}
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
