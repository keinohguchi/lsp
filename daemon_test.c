/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

extern pid_t _daemon(void);

int main(void)
{
	pid_t got, want;

	got = _daemon();
	if (got == -1) {
		perror("_daemon");
		return 1;
	}
	want = getsid(0);
	if (want == -1) {
		perror("getsid");
		return 1;
	}
	if (got != want) {
		fprintf(stderr, "unexpected session ID:\n- want: %d\n-  got: %d\n",
			want, got);
		return 1;
	}
	return 0;
}
