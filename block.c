/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>

/* map the logical block into physical block */
int get_phy_block(int fd, int logical_block)
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
int get_nr_blocks(int fd)
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
