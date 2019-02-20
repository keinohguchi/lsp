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
#include <sys/sysmacros.h>

/* ls program context */
static int version_flag = 0;
static int help_flag = 0;
static struct context {
	const char		*progname;
	struct winsize		win;
	int			colnum;
	int			colwidth;
	int			all:1;
	int			list:1;
	int			(*cmp)(const void *, const void *);
	const char		*const version;
	const char		*const opts;
	const struct option	lopts[];
} ctx = {
	.colwidth	= 20,		/* a fixed column width for now */
	.version	= "1.0.6",
	.opts		= "alr",
	.lopts		= {
			{"all",		no_argument,	NULL,		'a'},
			{"long",	no_argument,	NULL,		'l'},
			{"reverse",	no_argument,	NULL,		'r'},
			{"version",	no_argument,	&version_flag,	1},
			{"help",	no_argument,	&help_flag,	1},
			{},
	},
};

static void version(FILE *stream)
{
	fprintf(stream, "%s version %s\n", ctx.progname, ctx.version);
	exit(EXIT_SUCCESS);
}

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", ctx.progname, ctx.opts);
	fprintf(stream, "options:\n");
	for (o = ctx.lopts; o->name; o++) {
		fprintf(stream, "\t");
		if (!o->flag)
			fprintf(stream, "-%c,", o->val);
		fprintf(stream, "--%s:\t", o->name);
		switch (o->val) {
		case 'a':
			fprintf(stream,
				"do not ignore entries starting with .\n");
			break;
		case 'l':
			fprintf(stream, "use a long listing format\n");
			break;
		case 'r':
			fprintf(stream, "reverse order while sorting\n");
			break;
		case 1:
			switch (o->name[0]) {
			case 'v':
				fprintf(stream,
					"output version information and exit\n");
				continue;
			case 'h':
				fprintf(stream, "\tdisplay this help and exit\n");
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
	const char *const rwx[] = {
		"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx",
	};
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

static size_t print_file_long(const char *const base, const char *const file,
			      struct stat *restrict st)
{
	size_t len, total = 0;
	struct passwd *pwd;
	struct group *grp;
	char buf[BUFSIZ];
	struct tm tm;

	if (st == NULL) {
		struct stat sbuf;
		if (snprintf(buf, sizeof(buf), "%s/%s", base, file) < 0) {
			perror("snprintf");
			return -1;
		}
		if (lstat(buf, &sbuf) == -1) {
			perror("lstat");
			return -1;
		}
		st = &sbuf;
	}
	if (stmode(st->st_mode, buf, sizeof(buf)) == NULL)
	    return -1;
	if ((len = printf("%s %3ld", buf, st->st_nlink)) < 0) {
		perror("printf");
		return -1;
	}
	total += len;
	if ((pwd = getpwuid(st->st_uid)) != NULL) {
		if ((len = printf(" %-4s", pwd->pw_name)) < 0) {
			perror("printf");
			return -1;
		}
	} else {
		if ((len = printf(" %-4d", st->st_uid)) < 0) {
			perror("printf");
			return -1;
		}
	}
	total += len;
	if ((grp = getgrgid(st->st_gid)) != NULL) {
		if ((len = printf(" %-8s", grp->gr_name)) < 0) {
			perror("printf");
			return -1;
		}
	} else {
		if ((len = printf(" %-8d", st->st_gid)) < 0) {
			perror("printf");
			return -1;
		}
	}
	if (S_ISCHR(st->st_mode) || S_ISBLK(st->st_mode)) {
		if ((len = printf(" %4d,%4d", major(st->st_rdev),
				  minor(st->st_rdev))) < 0) {
			perror("printf");
			return -1;
		}
	} else {
		if ((len = printf(" %9jd", st->st_size)) < 0) {
			perror("printf");
			return -1;
		}
	}
	total += len;
	if (localtime_r(&st->st_mtime, &tm) == NULL) {
		perror("localtime_r");
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

static size_t print_file(const char *const base, const char *const file,
			 struct stat *restrict st)
{
	if (ctx.list)
		return print_file_long(base, file, st);
	else
		return printf("%-*s", ctx.colwidth, file);
}

static int ls_file(const char *const file, struct stat *restrict st)
{
	if (print_file("./", file, st) < 0)
		return -1;
	if (printf("\n") < 0)
		return -1;
	return 0;
}

static struct dirent *read_dir(const char *const path, size_t *nr)
{
	struct dirent *d, *dlist;
	size_t max = 8;
	DIR *dir;
	int i;

	*nr = 0;
	if ((dir = opendir(path)) == NULL) {
		perror("opendir");
		return NULL;
	}
	dlist = calloc(max, sizeof(struct dirent));
	if (dlist == NULL) {
		perror("calloc");
		goto out;
	}
	i = 0;
	while ((d = readdir(dir)) != NULL) {
		if (d->d_name[0] == '.' && !ctx.all)
			continue;
		if (i >= max) {
			struct dirent *dlist_new;
			max *= 2;
			dlist_new = realloc(dlist, sizeof(struct dirent)*max);
			if (dlist_new == NULL) {
				perror("realloc");
				free(dlist);
				dlist = NULL;
				goto out;
			}
			dlist = dlist_new;
		}
		dlist[i++] = *d;
	}
	*nr = i;
out:
	if (dir)
		if (closedir(dir) == -1) {
			perror("closedir");
			if (dlist)
				free(dlist);
			dlist = NULL;
		}
	return dlist;
}

static int ls_dir(const char *const path)
{
	struct dirent *dlist;
	int i, row, ret = -1;
	size_t nr;

	if ((dlist = read_dir(path, &nr)) == NULL)
		goto out;
	qsort(dlist, nr, sizeof(struct dirent), ctx.cmp);
	row = nr/ctx.colnum;
	if (nr%ctx.colnum)
		row++;
	for (i = 0; i < row; i++) {
		int j;
		for (j = 0; j < ctx.colnum && i+j*row < nr; j++)
			if (print_file(path, dlist[i+j*row].d_name, NULL) < 0)
				goto out;
		if (!ctx.list && printf("\n") < 0)
			goto out;
	}
	ret = 0;
out:
	if (dlist)
		free(dlist);
	return ret;
}

static int ls(const char *const file)
{
	struct stat st;
	char *path;
	int ret;

	if ((path = realpath(file, NULL)) == NULL) {
		perror("realpath");
		return -1;
	}
	if ((ret = lstat(path, &st)) == -1) {
		perror("lstat");
		goto out;
	}
	if (S_ISDIR(st.st_mode))
		ret = ls_dir(path);
	else
		ret = ls_file(file, &st);
out:
	if (path)
		free(path);
	return ret;
}

static int filecmp(const void *file1, const void *file2)
{
	const struct dirent *a = file1, *b = file2;
	return strcmp(a->d_name, b->d_name);
}

static int rfilecmp(const void *file1, const void *file2)
{
	const struct dirent *a = file2, *b = file1;
	return strcmp(a->d_name, b->d_name);
}

int main(int argc, char *const argv[])
{
	int i, opt, ret;

	ctx.progname = argv[0];
	ctx.cmp = filecmp;
	while ((opt = getopt_long(argc, argv, ctx.opts, ctx.lopts, &i)) != -1) {
		switch (opt) {
		case 0:
			switch (ctx.lopts[i].name[0]) {
			case 'v':
				version(stdout);
				break;
			case 'h':
				usage(stdout, EXIT_SUCCESS);
				break;
			}
			break;
		case 'a':
			ctx.all = 1;
			break;
		case 'l':
			ctx.list = 1;
			break;
		case 'r':
			ctx.cmp = rfilecmp;
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	/* multiple column support */
	ctx.colnum = 1;
	if (isatty(STDOUT_FILENO) && !ctx.list) {
		struct winsize win;
		int colnum;
		if ((ret = ioctl(STDOUT_FILENO, TIOCGWINSZ, &win)) == -1) {
			perror("ioctl");
			goto out;
		}
		if ((colnum = win.ws_col/ctx.colwidth) > 0)
			ctx.colnum = colnum;
	}
	/* let's rock */
	switch (argc - optind) {
	case 0:
		ret = ls(".");
		break;
	case 1:
		ret = ls(argv[optind]);
		break;
	default:
		while (optind < argc)
			if ((ret = ls(argv[optind++])) == -1)
				goto out;
		break;
	}
out:
	if (ret != 0)
		return 1;
	return 0;
}
