/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/resource.h>

static void usage(const char *app)
{
	printf("usage: %s [cpu|core|data|file|open] (soft limit)\n", app);
	exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
	extern int set_resource(int resource, const struct rlimit *rlim);
	extern int get_resource(int resource, struct rlimit *rlim);
	struct rlimit rlim;
	int resource;
	int ret;

	if (argc < 2)
		usage(argv[0]);

	if (!strcmp(argv[1], "cpu"))
		resource = RLIMIT_CPU;
	else if (!strcmp(argv[1], "core"))
		resource = RLIMIT_CORE;
	else if (!strcmp(argv[1], "data"))
		resource = RLIMIT_DATA;
	else if (!strcmp(argv[1], "file"))
		resource = RLIMIT_FSIZE;
	else if (!strcmp(argv[1], "open"))
		resource = RLIMIT_NOFILE;
	else
		usage(argv[0]);

	if (argc > 2) {
		long int val = strtol(argv[2], NULL, 10);
		if (val == -1) {
			perror("strtol");
			return 1;
		}
		rlim.rlim_cur = val;
		rlim.rlim_max = RLIM_INFINITY;
		ret = set_resource(resource, &rlim);
		if (ret == -1) {
			perror("setrlimit");
			return 1;
		}
	}
	ret = get_resource(resource, &rlim);
	if (ret == -1) {
		perror("getrlimit");
		return 1;
	}
	printf("%s has soft=%ld,hard=%ld limit\n",
	       argv[1], rlim.rlim_cur, rlim.rlim_max);
	return 0;
}
