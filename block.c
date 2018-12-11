/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

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

int main(int argc, char *argv[])
{
	int ret;
	int fd;

	if (argc < 2) {
		printf("usage: %s <filename>\n", argv[0]);
		return 0;
	}
	fd = open(argv[1], O_RDONLY);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	ret = get_nr_blocks(fd);
	if (ret == -1) {
		fprintf(stderr, "get_nr_blocks()\n");
		goto out;
	}
	printf("file(%s) consumes %d of blocks\n", argv[1], ret);
out:
	if (close(fd) == -1)
		perror("close");
	return ret;
}
