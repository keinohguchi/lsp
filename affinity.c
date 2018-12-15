/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <unistd.h>
#define __USE_GNU
#include <sched.h>

int getaffinity(cpu_set_t *mask)
{
	pid_t pid;
	int ret;

	pid = getpid();
	if (pid == -1) {
		perror("getpid");
		return -1;
	}
	ret = sched_getaffinity(pid, sizeof(cpu_set_t), mask);
	if (ret == -1) {
		perror("sched_getaffinity");
		return -1;
	}
	return 0;
}

int setaffinity(const cpu_set_t *mask)
{
	pid_t pid;
	int ret;

	pid = getpid();
	if (pid == -1) {
		perror("getpid");
		return -1;
	}
	ret = sched_setaffinity(pid, sizeof(cpu_set_t), mask);
	if (ret == -1) {
		perror("sched_setaffinity");
		return -1;
	}
	return 0;
}
