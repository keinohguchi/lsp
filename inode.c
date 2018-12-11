/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

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

int main(int argc, const char *argv[])
{
	int ret;
	int fd;

	if (argc < 2) {
		fprintf(stderr, "usage: %s <filename>\n", argv[0]);
		return 1;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	ret = get_inode(fd);
	if (ret == -1) {
		fprintf(stderr, "get_inode()\n");
		goto out;
	}
	printf("file=%s,inode=%d\n", argv[1], ret);
out:
	if (close(fd) == -1)
		perror("close");
	return ret;
}
