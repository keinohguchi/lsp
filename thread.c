/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>

static const char *progname;

struct thread {
	char		name[LINE_MAX];
	pthread_t	tid;
	int		status;
};

/* thread main */
static void *run(void *arg)
{
	const pthread_t me = pthread_self();
	struct thread *thing = arg;

	if (!pthread_equal(me, thing->tid)) {
		fprintf(stderr, "thread(%s): 0x%ld!=0x%ld\n",
			thing->name, me, thing->tid);
		thing->status = EXIT_FAILURE;
	} else {
		printf("thread(%s:%p): started\n", thing->name, &me);
		thing->status = EXIT_SUCCESS;
	}
	return arg;
}

static void usage(FILE *stream, int status, const char *opts,
		  const struct option *lopts)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,%s", o->val, o->name);
		switch (o->val) {
		case 'c':
			fprintf(stream, " N\tnumber of threads (default:1)\n");
			break;
		case 'h':
			fprintf(stream, "\t\tshow this message\n");
			break;
		default:
			fprintf(stream, "\n");
			break;
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	const char *opts = "c:h";
	const struct option lopts[] = {
		{"count", required_argument, NULL, 'c'},
		{"help",  no_argument, NULL, 'h'},
		{}, /* sentry */
	};
	struct thread *things;
	struct thread *thing, *retp;
	int opt;
	long int nr, i;
	int ret;

	progname = argv[0];
	nr = 1;
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
	       switch (opt) {
	       case 'c':
		       nr = strtol(optarg, NULL, 10);
		       if (nr <= 0 || nr >= LONG_MAX)
			       usage(stderr, EXIT_FAILURE, opts, lopts);
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

	things = calloc(nr, sizeof(struct thread));
	if (things == NULL) {
		perror("calloc");
		return 1;
	}
	ret = 0;
	for (i = 0; i < nr; i++) {
		thing = &things[i];
		ret = snprintf(thing->name, LINE_MAX, "Thing %ld", i+1);
		if (ret < 0) {
			perror("snprintf");
			goto out;
		}
		ret = pthread_create(&thing->tid, NULL, run, (void *)thing);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			nr = i;
			goto join;
		}
	}
join:
	for (i = 0; i < nr; i++) {
		thing = &things[i];
		if (pthread_join(thing->tid, (void **)&retp)) {
			perror("pthread_join");
			continue;
		}
		if (retp->status != EXIT_SUCCESS) {
			fprintf(stderr, "%s: exit error\n", retp->name);
			continue;
		}
		printf("%s: successfully exited\n", retp->name);
	}
out:
	free(things);
	return ret;
}
