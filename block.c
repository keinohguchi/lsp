/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

static const char *progname;

/* map the logical block into physical block */
static int get_phy_block(int fd, int logical_block)
{
	int ret;

	ret = ioctl(fd, FIBMAP, &logical_block);
	if (ret == -1) {
		perror("ioctl");
		return -1;
	}
	return logical_block;
}

/* get the number of blocks for the open file */
static int get_nr_blocks(int fd)
{
	struct stat buf;
	int ret;

	ret = fstat(fd, &buf);
	if (ret == -1) {
		perror("fstat");
		return -1;
	}
	return buf.st_blocks;
}

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;
	fprintf(stream, "usage %s [-%s] <file>\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t--%s,-%c:\t", o->name, o->val);
		switch (o->val) {
		case 'h':
			fprintf(stream, "show this message\n");
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	const struct option lopts[] = {
		{"help",	no_argument,	NULL,	'h'},
		{NULL,		0,		NULL,	0},
	};
	const char *opts = "h";
	const char *file;
	int opt, ret, fd, i;

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
		usage(stderr, EXIT_FAILURE, opts, lopts);

	file = argv[optind++];
	fd = open(file, O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	ret = get_nr_blocks(fd);
	if (ret == -1) {
		goto out;
	}
	for (i = 0; i < ret; i++) {
		int phy_block = get_phy_block(fd, i);
		if (phy_block == -1) {
			goto out;
		}
		if (!phy_block)
			continue;
		printf("file=%s,logical/physical=%03d/%d\n",
		       file, i, phy_block);
	}
out:
	if (close(fd) == -1)
		perror("close");
	return ret;
}
