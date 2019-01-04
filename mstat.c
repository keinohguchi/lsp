/* SPDX-License-Identifier: GPL-2.0 */
#include <unistd.h>
#include <malloc.h>

int main(int argc, char *argv[])
{
	size_t page = getpagesize();
	printf("pagesize=%ld\n", page);
	malloc_stats();
	return 0;
}
