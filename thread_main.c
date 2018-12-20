/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char *argv[])
{
	extern int start_threads(long int);
	long int nr = 1;
	int ret;

	if (argc > 1) {
		long int ret;

		errno = 0;
		ret = strtol(argv[1], NULL, 10);
		if (errno) {
			perror("strtol");
			return 1;
		}
		nr = ret;
	}
	ret = start_threads(nr);
	if (ret == -1)
		return 1;
	return 0;
}
