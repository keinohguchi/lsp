/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
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

static void usage(FILE *stream, int status)
{
	fprintf(stream, "usage: %s <filename>\n", progname);
	exit(status);
}

int main(int argc, const char *argv[])
{
	int ret;
	int fd;

	progname = argv[0];
	if (argc < 2)
		usage(stdout, EXIT_SUCCESS);

	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		exit(EXIT_FAILURE);
	}
	ret = get_inode(fd);
	if (ret == -1) {
		ret = 1;
		fprintf(stderr, "cannot get inode: %s\n", argv[1]);
		goto out;
	}
	printf("file=%s,inode=%d\n", argv[1], ret);
	ret = 0;
out:
	if (close(fd) == -1)
		perror("close");
	return ret;
}
