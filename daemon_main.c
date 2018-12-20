/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

extern pid_t xdaemon(void);

int main(void)
{
	xdaemon();
	return 0;
}
