/* SPDX-License-Identifier: GPL-2.0 */
#include <sys/time.h>
#include <sys/resource.h>

int get_resource(int resource, struct rlimit *rlim)
{
	return getrlimit(resource, rlim);
}

int set_resource(int resource, const struct rlimit *rlim)
{
	return setrlimit(resource, rlim);
}
