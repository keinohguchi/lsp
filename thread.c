/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>

struct thread {
	char		name[LINE_MAX];
	pthread_t	tid;
};

void *main_thread(void *arg)
{
	const pthread_t me = pthread_self();
	struct thread *thing = arg;

	if (!pthread_equal(me, thing->tid))
		fprintf(stderr, "thread(%s): 0x%ld!=0x%ld\n",
			thing->name, me, thing->tid);
	else
		printf("thread(%s:0x%ld): started\n", thing->name, me);

	return arg;
}

int start_threads(long int nr)
{
	struct thread *things;
	struct thread *thing, *retp;
	long int i;
	int ret = 0;

	things = calloc(nr, sizeof(struct thread));
	if (things == NULL) {
		perror("calloc");
		return -1;
	}
	for (i = 0; i < nr; i++) {
		thing = &things[i];
		ret = snprintf(thing->name, LINE_MAX, "Thing %ld", i+1);
		if (ret < 0) {
			perror("snprintf");
			goto out;
		}
		ret = pthread_create(&thing->tid, NULL, main_thread,
				     (void *)thing);
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
			errno = ret;
			perror("pthread_join");
			continue;
		}
		printf("thread(%s): joined\n", retp->name);
	}
out:
	free(things);
	return ret;
}
