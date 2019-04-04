/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
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

static struct process {
	const char		*pattern;
	int			recursive:1;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.recursive	= 0,
	.opts		= "n:rh",
	.lopts		= {
		{"name",	required_argument,	NULL,	'n'},
		{"help",	no_argument,		NULL,	'h'},
		{"recursive",	no_argument,		NULL,	'r'},
		{NULL, 0, NULL, 0},
	},
};

/* context used for the thread communication */
struct context {
	const struct process	*p;
	const char		*path;
	pthread_t		tid;
	int			ret;
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s] <directory name>\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'n':
			fprintf(s, "\tfind specified pattern\n");
			break;
		case 'r':
			fprintf(s, "\trecursively find the file\n");
			break;
		case 'h':
			fprintf(s, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
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

static int pathmatch(const struct process *restrict p, const char *restrict path)
{
	char *tmp, *name;
	int ret;
	tmp = strdup(path);
	if (tmp == NULL) {
		perror("strdup");
		return -1;
	}
	name = basename(tmp);
	ret = fnmatch(p->pattern, name, FNM_EXTMATCH);
	free(tmp);
	return ret;
}

static int find(const struct process *restrict p, const char *const path);

static void *finder(void *data)
{
	struct context *ctx = data;
	ctx->ret = find(ctx->p, ctx->path);
	return (void *)&ctx->ret;
}

static int find(const struct process *restrict p, const char *const path)
{
	struct context ctxs[1024];
	struct dirent *d;
	struct stat s;
	DIR *dir;
	int ret;
	int i;

	if (!pathmatch(p, path))
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
		if (p->recursive) {
			ret = find(p, child);
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
			ctx->p = p;
			ret = pthread_create(&ctx->tid, NULL, finder, ctx);
			if (ret) {
				errno = ret;
				perror("pthread_cretae");
				break;
			}
			i++;
		}
		if (ret)
			break;
	}
	if (closedir(dir))
		perror("closedir");
	if (!p->recursive) {
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
	struct process *p = &process;
	const char *path;
	int o;

	p->progname = argv[0];
	p->pattern = "*";
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1)
		switch (o) {
		case 'n':
			p->pattern = optarg;
			break;
		case 'r':
			p->recursive = 1;
			break;
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	if (optind >= argc)
		usage(p, stderr, EXIT_FAILURE);

	/* let's roll */
	path = argv[optind];
	if (find(p, path))
		return 1;
	return 0;
}
