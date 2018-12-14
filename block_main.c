/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>

extern int get_nr_blocks(int fd);
extern int get_phy_block(int fd, int logical_block);

int main(int argc, char *argv[])
{
	const char *path;
	int ret;
	int fd;
	int i;

	if (argc < 2) {
		printf("usage: %s <filename>\n", argv[0]);
		return 0;
	}
	path = argv[1];
	fd = open(path, O_RDONLY);
	if (fd == -1) {
		perror("open");
		return 1;
	}
	ret = get_nr_blocks(fd);
	if (ret == -1) {
		fprintf(stderr, "get_nr_blocks()\n");
		goto out;
	}
	for (i = 0; i < ret; i++) {
		int phy_block = get_phy_block(fd, i);
		if (phy_block == -1) {
			fprintf(stderr, "get_phy_block() error\n");
			ret = 1;
			goto out;
		}
		if (!phy_block)
			continue;
		printf("file=%s,logical/physical=%03d/%d\n",
		       path, i, phy_block);
	}
out:
	if (close(fd) == -1)
		perror("close");
	return ret;
}
