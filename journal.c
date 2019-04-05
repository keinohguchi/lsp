/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <systemd/sd-journal.h>

struct context {
	sd_journal		*jd;
	char			*cursor;
	size_t			cursor_len;
};

static struct process {
	struct context		journal[1];	/* single context */
	const char		*unit;
	const char		*cursor_file;
	short			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.journal[0].jd		= NULL,
	.journal[0].cursor	= NULL,
	.journal[0].cursor_len	= 0,
	.unit			= NULL,
	.cursor_file		= NULL,
	.timeout		= 0,
	.progname		= NULL,
	.opts			= "u:f:t:h",
	.lopts			= {
		{"unit",	required_argument,	NULL,	'u'},
		{"cursor_file",	required_argument,	NULL,	'f'},
		{"timeout",	required_argument,	NULL,	't'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s", o->val, o->name);
		switch (o->val) {
		case 'u':
			fprintf(s, "\t\tShow logs from the specified service unit\n");
			break;
		case 'f':
			fprintf(s, "\tPersistent journal cursor file (default: none)\n");
			break;
		case 't':
			fprintf(s, "\t\tTimeout in millisecond, "
				"in case of interval option (default: none)\n");
			break;
		case 'h':
			fprintf(s, "\t\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t\t%s option", o->name);
			break;
		}
	}
	exit(status);
}

static int init_cursor(struct process *p)
{
	struct context *ctx = p->journal;
	int ret, fd = -1;
	struct stat st;
	void *cursor;
	size_t len;

	ctx->cursor_len = 0;
	ctx->cursor = NULL;
	if (p->cursor_file == NULL)
		return 0;

	len = sysconf(_SC_LINE_MAX);
	if (len == -1) {
		perror("sysconf(_SC_LINE_MAX)");
		return -1;
	}
	fd = open(p->cursor_file, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	ret = fstat(fd, &st);
	if (ret == -1) {
		perror("fstat");
		goto err;
	}
	if (st.st_size < len) {
		ret = ftruncate(fd, len);
		if (ret == -1) {
			perror("ftruncate");
			goto err;
		}
	}
	cursor = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (cursor == NULL) {
		perror("mmap");
		goto err;
	}
	if (close(fd)) {
		if (munmap(cursor, len))
			perror("munmap");
		perror("close");
		return -1;
	}
	ctx->cursor_len = len;
	ctx->cursor = cursor;
	return 0;
err:
	if (fd != -1)
		if (close(fd))
			perror("close");
	return -1;
}

static int init(struct process *p)
{
	struct context *ctx = p->journal;
	int ret;

	ret = init_cursor(p);
	if (ret == -1)
		return -1;
	ret = sd_journal_open(&ctx->jd, SD_JOURNAL_LOCAL_ONLY);
	if (ret < 0) {
		errno = -ret;
		perror("sd_journal_open");
		goto err;
	}
	if (p->unit) {
		char buf[LINE_MAX];
		snprintf(buf, sizeof(buf), "_SYSTEMD_UNIT=%s.service", p->unit);
		ret = sd_journal_add_match(ctx->jd, buf, 0);
		if (ret < 0) {
			errno = -ret;
			perror("sd_journal_add_match");
			goto err;
		}
	}
	ret = sd_journal_seek_head(ctx->jd);
	if (ret < 0) {
		errno = -ret;
		perror("sd_journal_seek_head");
		goto err;
	}
	return 0;
err:
	if (ctx->jd)
		sd_journal_close(ctx->jd);
	if (ctx->cursor != NULL)
		if (munmap(ctx->cursor, ctx->cursor_len))
			perror("munmap");
	return -1;
}

static void term(const struct process *restrict p)
{
	const struct context *ctx = p->journal;
	if (ctx->jd)
		sd_journal_close(ctx->jd);
	if (ctx->cursor)
		munmap(ctx->cursor, ctx->cursor_len);
	if (p->cursor_file)
		free((void *)p->cursor_file);
	if (p->unit)
		free((void *)p->unit);
}

static int fetch(struct context *ctx)
{
	return sd_journal_next(ctx->jd);
}

static int handle(struct context *ctx)
{
	const char *data;
	size_t len;
	int ret;

	ret = sd_journal_get_data(ctx->jd, "MESSAGE", (const void **)&data, &len);
	if (ret < 0) {
		errno = -ret;
		perror("sd_journal_get_data");
		return -1;
	}
	return printf("%*s\n", (int)len, data);
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	struct context *ctx;
	int ret, o;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (o) {
		case 'u':
			p->unit = strdup(optarg);
			if (p->unit == NULL) {
				perror("strdup");
				usage(p, stderr, EXIT_FAILURE);
			}
			break;
		case 'f':
			p->cursor_file = strdup(optarg);
			if (p->cursor_file == NULL) {
				perror("strdup");
				usage(p, stderr, EXIT_FAILURE);
			}
			break;
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < -1 || val > SHRT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = val;
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
	ret = init(p);
	if (ret == -1)
		return 1;
	ctx = p->journal;
	while ((ret = fetch(ctx)) > 0)
		if ((ret = handle(ctx)) == -1)
			break;
	term(p);
	return 0;
}
