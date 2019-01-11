/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <sched.h>
#include <unistd.h>
#include <stdlib.h>

static int getaffinity(cpu_set_t *mask)
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

int main(int argc, char *argv[])
{
	cpu_set_t mask;
	int ret, i;

	ret = getaffinity(&mask);
	if (ret == -1)
		exit(EXIT_FAILURE);

	printf("CPU_SETSIZE=%d\n", CPU_SETSIZE);
	for (i = 0; i < CPU_SETSIZE; i++)
		if (CPU_ISSET(i, &mask))
			printf("cpu=%d is bound\n", i);

	return 0;
}
