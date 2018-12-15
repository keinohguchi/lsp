/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#define __USE_GNU
#include <sched.h>

int main(int argc, char *argv[])
{
	extern int setaffinity(const cpu_set_t *mask);
	extern int getaffinity(cpu_set_t *mask);
	cpu_set_t mask;
	int ret, i;

	ret = getaffinity(&mask);
	if (ret == -1)
		return 1;

	printf("CPU_SETSIZE=%d\n", CPU_SETSIZE);
	for (i = 0; i < CPU_SETSIZE; i++)
		if (CPU_ISSET(i, &mask))
			printf("cpu=%d is bound\n", i);

	return 0;
}
