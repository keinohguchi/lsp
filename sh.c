/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

static const unsigned default_timeout = 10;
static const char *progname;
static const char *const opts = "t:p:h";
static const struct option lopts[] = {
	{"timeout",	required_argument,	NULL,	't'},
	{"prompt",	required_argument,	NULL,	'p'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(stream, "\tspecify timeout in second\n");
			break;
		case 'p':
			fprintf(stream, "\tspecify the prompt\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option", o->name);
			break;
		}
	}
	exit(status);
}

static void timeout_handler(int signo)
{
	if (signo != SIGALRM)
		return;
	printf("\n");
	exit(EXIT_SUCCESS);
}

static int init_timeout(unsigned timeout)
{
	signal(SIGALRM, timeout_handler);
	while (timeout)
		timeout = alarm(timeout);
	return 0;
}

int main(int argc, char *const argv[])
{
	unsigned timeout = default_timeout;
	const char *prompt = "sh";
	char *cmd, line[LINE_MAX];
	int ret, opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val == LONG_MAX)
				usage(stderr, EXIT_FAILURE);
			timeout = (unsigned)val;
			break;
		case 'p':
			prompt = optarg;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = init_timeout(timeout);
	if (ret == -1)
		return 1;
	printf("%s$ ", prompt);
	while ((cmd = fgets(line, sizeof(line), stdin)))
		printf("%s$ ", prompt);
	printf("\n");
	return 0;
}
