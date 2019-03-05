/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <libgen.h>
#include <string.h>
#include <fnmatch.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

static const char *progname;
static const char *pattern;
static bool recursive = false;
static const char *opts = "n:rh";
static const struct option lopts[] = {
	{"name",	required_argument,	NULL,	'n'},
	{"help",	no_argument,		NULL,	'h'},
	{"recursive",	no_argument,		NULL,	'r'},
	{NULL,		0,			NULL,	0},
};

/* context used for the thread communication */
struct context {
	const char	*path;
	pthread_t	tid;
	int		ret;
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] <directory name>\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'n':
			fprintf(stream, "\tfind specified pattern\n");
			break;
		case 'r':
			fprintf(stream, "\trecursively find the file\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	_exit(status);
}

static const char *pathname(const char *base, const char *file, char *buf, size_t len)
{
	switch (strlen(base)) {
	case 0:
		return file;
	default:
		if (base[strlen(base)-1] == '/') {
			if (snprintf(buf, len, "%s%s", base, file) < 0)
				return file;
		} else {
			if (snprintf(buf, len, "%s/%s", base, file) < 0)
				return file;
		}
		return buf;
	}
}

static int pathmatch(const char *restrict path, const char *restrict pattern)
{
	char *tmp, *name;
	int ret;
	tmp = strdup(path);
	if (tmp == NULL) {
		perror("strdup");
		return -1;
	}
	name = basename(tmp);
	ret = fnmatch(pattern, name, FNM_EXTMATCH);
	free(tmp);
	return ret;
}

static int find(const char *const path);

static void *finder(void *data)
{
	struct context *ctx = data;
	ctx->ret = find(ctx->path);
	return (void *)&ctx->ret;
}

static int find(const char *const path)
{
	struct context ctxs[1024];
	struct dirent *d;
	struct stat s;
	DIR *dir;
	int ret;
	int i;

	if (!pathmatch(path, pattern))
		printf("%s\n", path);

	ret = lstat(path, &s);
	if (ret == -1) {
		perror("lstat");
		return -1;
	}
	if (!S_ISDIR(s.st_mode))
		return 0;

	dir = opendir(path);
	if (dir == NULL) {
		/* ignore this directory */
		perror("opendir");
		return 0;
	}
	i = 0;
	while ((d = readdir(dir))) {
		char *child;
		if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
			continue;
		child = malloc(PATH_MAX);
		if (child == NULL) {
			perror("malloc");
			ret = -1;
			break;
		}
		pathname(path, d->d_name, child, PATH_MAX);
		if (recursive) {
			ret = find(child);
			free(child);
		} else {
			struct context *ctx;
			if (i >= 1024) {
				fprintf(stderr, "too many child directories\n");
				ret = -1;
				break;
			}
			ctx = &ctxs[i];
			ctx->path = child;
			ret = pthread_create(&ctx->tid, NULL, finder, ctx);
			if (ret) {
				errno = ret;
				perror("pthread_cretae");
				free(child);
			}
			i++;
		}
		if (ret)
			break;
	}
	if (closedir(dir))
		perror("closedir");
	if (!recursive) {
		while (--i >= 0) {
			int *retp;
			int cret = pthread_join(ctxs[i].tid, (void **)&retp);
			if (cret) {
				errno = cret;
				perror("pthread_join");
				ret = -1;
			}
			if (*retp) {
				fprintf(stderr, "pthread returns error: %d\n", *retp);
				ret = *retp;
			}
			free((void *)ctxs[i].path);
		}
	}
	return ret;
}

int main(int argc, char *argv[])
{
	const char *path;
	int opt;

	progname = argv[0];
	pattern = "*";
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1)
		switch (opt) {
		case 'n':
			pattern = optarg;
			break;
		case 'r':
			recursive = true;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	if (optind >= argc)
		usage(stderr, EXIT_FAILURE);

	/* let's roll */
	path = argv[optind];
	if (find(path))
		return 1;
	return 0;
}
