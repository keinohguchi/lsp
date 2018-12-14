/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

extern pid_t _daemon(void);

int main(void)
{
	_daemon();
}
