/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/time.h>
#include <sys/resource.h>

static const char *progname;

int set_resource(int resource, const struct rlimit *rlim)
{
	return setrlimit(resource, rlim);
}

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *lopts)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s] [resource value]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s: ", o->val, o->name);
		switch (o->val) {
		case 'a':
			fprintf(stream, "get all the resources\n");
			break;
		case 'h':
			fprintf(stream, "show this message\n");
			break;
		case 'l':
			fprintf(stream, "get specified resourc(es)\n");
			break;
		default:
			fprintf(stream, "set/get %s resource\n", o->name);
			break;
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	const char *const opts = "acdfhlop";
	const struct option lopts[] = {
		{ "all",  no_argument, NULL, 'a'},
		{ "core", no_argument, NULL, 'c'},
		{ "data", no_argument, NULL, 'd'},
		{ "file", no_argument, NULL, 'f'},
		{ "open", no_argument, NULL, 'o'},
		{ "cpu",  no_argument, NULL, 'p'},
		{ "list", no_argument, NULL, 'l'},
		{ "help", no_argument, NULL, 'h'},
		{}, /* sentry */
	};
	long int opt;
	int resources[RLIMIT_NLIMITS];
	int resource_index = 0;
	bool list = false;
	int i;

	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'a':
			list = true;
			for (i = 0; i < RLIMIT_NLIMITS; i++)
				resources[resource_index++] = i;
			break;
		case 'c':
			resources[resource_index++] = RLIMIT_CORE;
			break;
		case 'd':
			resources[resource_index++] = RLIMIT_DATA;
			break;
		case 'f':
			resources[resource_index++] = RLIMIT_FSIZE;
			break;
		case 'o':
			resources[resource_index++] = RLIMIT_NOFILE;
			break;
		case 'p':
			resources[resource_index++] = RLIMIT_CPU;
			break;
		case 'l':
			list = true;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}
	/* get all the resource if there is no options */
	if (resource_index == 0) {
		list = true;
		for (i = 0; i < RLIMIT_NLIMITS; i++)
			resources[resource_index++] = i;
	}
	for (i = 0; i < resource_index; i++) {
		struct rlimit rlim;
		if (!list)
			continue;
		if (getrlimit(resources[i], &rlim) == -1) {
			perror("getrlimit");
			abort();
		}
		printf("resource=%d,soft=%ld,hard=%ld\n",
		       resources[i], rlim.rlim_cur, rlim.rlim_max);
	}
	return 0;
}
