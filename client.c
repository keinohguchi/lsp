/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <limits.h>
#include <getopt.h>

struct client {
	char			buf[LINE_MAX];
	const struct process	*p;
};

static struct process {
	struct client		client[1];		/* single client */
	const char		*progname;
	const char		*prompt;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.progname	= NULL,
	.prompt		= "client",
	.opts		= "h",
	.lopts		= {
		{"help",	no_argument,	NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'h':
			fprintf(s, "\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int is_print_prompt(const struct process *restrict p)
{
	return p->prompt[0] != '\0';
}

static void prompt(const struct process *restrict p)
{
	if (is_print_prompt(p))
		printf("%s> ", p->prompt);
}

static char *fetch(struct client *ctx)
{
	prompt(ctx->p);
	return fgets(ctx->buf, sizeof(ctx->buf), stdin);
}

static int handle(struct client *ctx, const char *cmdline)
{
	if (!strncasecmp(cmdline, "quit", strlen(cmdline)-1))
		return 0;
	printf("%s", cmdline);
	return 1;
}

static int init(struct process *p)
{
	struct client *c = p->client;
	c->p = p;
	return 0;
}

static void term(const struct process *restrict p)
{
	return;
}

int main(int argc, char *const argv[])
{
	struct process *const p = &process;
	const char *cmd;
	int o, ret;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		switch (o) {
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
	while ((cmd = fetch(p->client)))
		if ((ret = handle(p->client, cmd)) <= 0)
			break;
	term(p);
	if (ret)
		return 1;
	return 0;
}
