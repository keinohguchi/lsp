/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ucontext.h>

/* daemon information. */
static struct daemon_info {
	sig_atomic_t	pid;
	sig_atomic_t	cookie;
} daemon_info = {
	.pid	= 0,
	.cookie	= 0,
};

/* retrieving the daemon's state */
static void handler(int signo, siginfo_t *si, void *context)
{
	ucontext_t *ctx = context;
	int i;

	/* signal number */
	if (signo != SIGUSR1 || si->si_signo != SIGUSR1) {
		fprintf(stderr,
			"unexpected signal:\n- want: %s\n-  got: %s\n",
			sys_siglist[SIGUSR1], sys_siglist[signo]);
		exit(EXIT_FAILURE);
	}
	/* signal should be raised by sigqueue() */
	if (si->si_code != SI_QUEUE) {
		fprintf(stderr,
			"unexpected signal code:\n- want: %#x\n-  got: %#x\n",
			SI_QUEUE, si->si_code);
		exit(EXIT_FAILURE);
	}
	/* make sure sigval contains the daemon's pid */
	if (si->si_int != si->si_pid) {
		fprintf(stderr,
			"unexpected pid:\n- want: %d\n-  got: %d\n",
			si->si_pid, si->si_value.sival_int);
		exit(EXIT_FAILURE);
	}
	/* check signal mask */
	for (i = 0; i < _NSIG; i++) {
	     if (sys_siglist[i] == NULL)
		     continue;
	     fprintf(stderr, "%s %s masked\n",
		     sys_siglist[i],
		     sigismember(&ctx->uc_sigmask, i) ? "is" : "is not");
	}
	/*
	 * As we mask all the signals, we can use global variable
	 * to pass the daemon pid around.
	 */
	daemon_info.pid = si->si_pid;
	daemon_info.cookie = si->si_int;
}

static int shutdown(struct daemon_info *info)
{
	/* Returns the cookie back */
	union sigval val = { .sival_int = info->cookie };
	int status;

	/* terminate the daemon */
	if (sigqueue(info->pid, SIGTERM, val) == -1) {
		perror("sigqueue");
		abort();
	}

	/* wait for the daemon to exit */
	if (waitpid(info->pid, &status, WNOHANG) == -1) {
		/* daemon is gone */
		if (errno == ECHILD)
			return 0;
		perror("waitpid");
		abort();
	}
	if (WIFEXITED(status))
		return WEXITSTATUS(status);
	if (!WIFSIGNALED(status))
		return -1;
	return 0;
}

static char *const pidstring(pid_t pid)
{
	static char buf[LINE_MAX];
	if (snprintf(buf, sizeof(buf), "%d", pid) == -1) {
		perror("snprintf");
		abort();
	}
	return buf;
}

static char *const progpath(const char *const prog)
{
	static char buf[LINE_MAX];
	size_t len;
	char *endp;

	if (getcwd(buf, sizeof(buf)) == NULL) {
		perror("getcwd");
		abort();
	}
	len = strlen(buf);
	endp = &buf[len];
	if (snprintf(endp, LINE_MAX-len, "/%s", prog) < 0) {
		perror("snprintf");
		abort();
	}
	return buf;
}

int main(void)
{
	char *const pid_str = pidstring(getpid());
	char *const target = progpath("daemon");
	const struct test {
		const char	*name;
		char		*const argv[4];
	} tests[] = {
		{
			.name	= "-p option",
			.argv	= { target, "-p", pid_str, NULL},
		},
		{ .name = NULL }, /* sentry */
	};
	const struct test *t;
	struct sigaction sa = {
		.sa_sigaction	= handler,
		.sa_flags	= SA_SIGINFO,
	};
	if (sigfillset(&sa.sa_mask) == -1) {
		perror("sigfillset");
		abort();
	}
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("sigaction");
		abort();
	}

	for (t = tests; t->name; t++) {
		pid_t pid;
		int ret;

		/* run daemon */
		pid = fork();
		if (pid == -1) {
			perror("fork");
			abort();
		} else if (pid == 0) {
			if (execv(target, t->argv) == -1) {
				perror("execv");
				abort();
			}
			/* won't reach */
		}

		/* wait for the daemon to send signal back to us. */
		for (;;)
			if (pause() == -1) {
				if (errno != EINTR) {
					perror("pause");
					abort();
				}
				break;
			}

		/* terminate the daemon */
		if ((ret = shutdown(&daemon_info)) != 0)
			return ret;
	}
	return 0;
}
