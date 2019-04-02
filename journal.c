/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <systemd/sd-journal.h>

struct context {
	sd_journal *jd;
};

static struct process {
	struct context		journal[1];	/* single context */
	const char		*unit;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.journal[0].jd	= NULL,
	.unit		= NULL,
	.progname	= NULL,
	.opts		= "u:h",
	.lopts		= {
		{"unit",	required_argument,	NULL,	'u'},
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
			fprintf(s, "\tShow logs from the specified service unit\n");
			break;
		case 'h':
			fprintf(s, "\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option", o->name);
			break;
		}
	}
	exit(status);
}

static int init(struct process *p)
{
	struct context *ctx = p->journal;
	int ret;

	ret = sd_journal_open(&ctx->jd, SD_JOURNAL_LOCAL_ONLY);
	if (ret < 0) {
		errno = -ret;
		perror("sd_journal_open");
		return -1;
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
	sd_journal_close(ctx->jd);
	return -1;
}

static void term(const struct process *restrict p)
{
	const struct context *ctx = p->journal;
	if (ctx->jd)
		sd_journal_close(ctx->jd);
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
		switch (o) {
		case 'u':
			p->unit = strdup(optarg);
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
