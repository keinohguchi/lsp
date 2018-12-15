/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#define __USE_GNU
#include <sched.h>

int main(void)
{
	const struct test {
		char	*name;
		int	cpu;
	} tests[] = {
		{
			.name	= "CPU0",
			.cpu	= 0,
		},
		{
			.name	= "CPU1",
			.cpu	= 1,
		},
		{
			.name	= "CPU2",
			.cpu	= 2,
		},
		{
			.name	= "CPU3",
			.cpu	= 3,
		},
		{
			.name	= "CPU4",
			.cpu	= 4,
		},
		{
			.name	= "CPU5",
			.cpu	= 5,
		},
		{
			.name	= "CPU6",
			.cpu	= 6,
		},
		{
			.name	= "CPU7",
			.cpu	= 7,
		},
		{},	/* sentry */
	};
	extern int setaffinity(const cpu_set_t *mask);
	extern int getaffinity(cpu_set_t *mask);
	const struct test *t;
	cpu_set_t mask;
	int ret;

	ret = getaffinity(&mask);
	if (ret == -1)
		return 1;

	for (t = tests; t->name; t++) {
		cpu_set_t got, want;
		int ret, i;

		/* skip tests if it's out of CPU set */
		if (!CPU_ISSET(t->cpu, &mask))
			continue;

		/* set the affinity to the specific CPU */
		CPU_ZERO(&want);
		CPU_SET(t->cpu, &want);
		ret = setaffinity(&want);
		if (ret == -1)
			goto fail;

		/* get the current affinity */
		ret = getaffinity(&got);
		if (ret == -1)
			goto fail;

		/* check if the affinity is what we want */
		for (i = 0; i < CPU_SETSIZE; i++)
			if (CPU_ISSET(i, &got) != CPU_ISSET(i, &want))
				goto fail;
	}
	return 0;
fail:
	printf("%s: ", t->name);
	return 1;
}
