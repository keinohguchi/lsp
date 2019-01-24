/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <getopt.h>
#include <sched.h>

static const char *progname;

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

static void usage(FILE *stream, int status, const char *const opts)
{
	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	exit(status);
}

int main(int argc, char *argv[])
{
	const char *opts = "c:hl";
	cpu_set_t mask;
	long int cpu = -1;
	bool list = false;
	int ret, i, opt;

	progname = argv[0];
	while ((opt = getopt(argc, argv, opts)) != -1) {
		switch (opt) {
		case 'c':
			cpu = strtol(optarg, NULL, 10);
			if (cpu < 0 || cpu >= LONG_MAX)
				usage(stderr, EXIT_FAILURE, opts);
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts);
			break;
		case 'l':
			list = true;
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts);
			break;
		}
	}

	CPU_ZERO(&mask);
	if (cpu != -1) {
		CPU_SET(cpu, &mask);
		if (setaffinity(&mask) == -1)
			return 1;
		if (!list)
			return 0;
	}
	ret = getaffinity(&mask);
	if (ret == -1)
		return 1;

	printf("CPU_SETSIZE=%d\n", CPU_SETSIZE);
	for (i = 0; i < CPU_SETSIZE; i++)
		if (CPU_ISSET(i, &mask))
			printf("cpu=%d is bound\n", i);

	return 0;
}
