/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>

static const char *progname;

/* get inode number for the open file */
static int get_inode(int fd)
{
	struct stat buf;
	int ret;

	ret = fstat(fd, &buf);
	if (ret == -1) {
		perror("fstat");
		return -1;
	}
	return buf.st_ino;
}

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;

	fprintf(stream, "usage: %s [-%s] <filename>\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++)
		switch (o->val) {
		case 'h':
			fprintf(stream, "\t--%s,-%c:\tshow this message\n",
				o->name, o->val);
			break;
		}
	exit(status);
}

int main(int argc, char *const argv[])
{
	const char *const opts = "h";
	const struct option lopts[] = {
		{"help",	no_argument,	NULL,	'h'},
		{}, /* sentry */
	};
	const char *file;
	int ret, opt, fd;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	if (optind >= argc)
		usage(stdout, EXIT_SUCCESS, opts, lopts);

	file = argv[optind++];
	fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	ret = get_inode(fd);
	if (ret == -1) {
		ret = 1;
		fprintf(stderr, "cannot get inode: %s\n", file);
		goto out;
	}
	printf("file=%s,inode=%d\n", file, ret);
	ret = 0;
out:
	if (close(fd) == -1)
		perror("close");
	return ret;
}
