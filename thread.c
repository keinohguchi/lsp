/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>

struct thread {
	char		name[LINE_MAX];
	pthread_t	tid;
	int		status;
};

static struct process {
	long		concurrent;
	int		kill;
	const char	*progname;
	const char	*const opts;
	const struct option	lopts[];
} process = {
	.concurrent	= 1,
	.kill		= -1,
	.progname	= NULL,
	.opts		= "c:k:h",
	.lopts		= {
		{"concurrent",	required_argument,	NULL,	'c'},
		{"kill",	required_argument,	NULL,	'k'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s", o->val, o->name);
		switch (o->val) {
		case 'c':
			fprintf(s, "\tnumber of threads (default: %ld)\n",
				p->concurrent);
			break;
		case 'k':
			fprintf(s, "\tSend signal to thread before join (default: off)\n");
			break;
		case 'h':
			fprintf(s, "\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

/* runner thread */
static void *runner(void *arg)
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

int main(int argc, char *argv[])
{
	struct process *p = &process;
	struct thread *things;
	struct thread *thing, *retp;
	int ret, i, j, o;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (o) {
		case 'c':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val >= LONG_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->concurrent = val;
			break;
		case 'k':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val > SIGRTMAX)
				usage(p, stderr, EXIT_FAILURE);
			p->kill = val;
			break;
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	}

	things = calloc(p->concurrent, sizeof(struct thread));
	if (things == NULL) {
		perror("calloc");
		return 1;
	}
	ret = 0;
	for (i = 0; i < p->concurrent; i++) {
		thing = &things[i];
		ret = snprintf(thing->name, LINE_MAX, "Thing %d", i+1);
		if (ret < 0) {
			perror("snprintf");
			goto out;
		}
		ret = pthread_create(&thing->tid, NULL, runner, (void *)thing);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			goto join;
		}
	}
join:
	for (j = 0; j < i; j++) {
		thing = &things[j];
		if (p->kill != -1) {
			if ((ret = pthread_kill(thing->tid, p->kill))) {
				errno = ret;
				perror("pthread_kill");
				/* ignore the error */
			}
		}
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
