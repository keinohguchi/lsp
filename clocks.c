/* SPDX-License-Identifier: GPL-2.0 */
#include <time.h>
#include <stdio.h>

int main(void)
{
	const clockid_t clocks[] = {
		CLOCK_REALTIME,
		CLOCK_MONOTONIC,
		CLOCK_MONOTONIC_RAW,
		CLOCK_PROCESS_CPUTIME_ID,
		CLOCK_THREAD_CPUTIME_ID,
		-1, /* sentry */
	};
	int i;

	for (i = 0; clocks[i] != -1; i++) {
		struct timespec res;
		int ret;

		ret = clock_getres(clocks[i], &res);
		if (ret == -1)
			perror("clock_getres");
		else
			printf("clock=%d res.tv_time=%ld res.tv_nsec=%ld\n",
			       clocks[i], res.tv_sec, res.tv_nsec);
	}
	return 0;
}
