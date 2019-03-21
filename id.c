/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

static const char *progname;
static const char *const opts = "h";
static const struct option lopts[] = {
	{"help",	no_argument,	NULL,	'h'},
	{NULL,		0,		NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *const argv[])
{
	struct passwd *p;
	struct group *g;
	uid_t uid;
	gid_t gid;
	int opt;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	uid = geteuid();
	p = getpwuid(uid);
	if (p == NULL) {
		perror("getpwuid");
		/* ignore */
	}
	gid = getegid();
	g = getgrgid(gid);
	if (g == NULL) {
		perror("getgrgid");
		/* ignore */
	}
	printf("uid=%d(%s) gid=%d(%s)\n", uid, p ? p->pw_name : "null", gid, g ? g->gr_name : "null");
	return 0;
}
