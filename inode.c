/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <sys/stat.h>

int get_inode(int fd)
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
