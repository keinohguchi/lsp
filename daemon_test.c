/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <signal.h>
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
	daemon_info.pid = si->si_int;
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
		return -1;
	}

	/* wait for the daemon to exit */
	if (waitpid(info->pid, &status, WNOHANG) == -1) {
		/* daemon is gone */
		if (errno == ECHILD)
			return 0;
		perror("waitpid");
		return -1;
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

int main(void)
{
	char *const pid_str = pidstring(getpid());
	char *const target = "./daemon";
	const struct test {
		const char	*name;
		char		*const argv[5];
	} tests[] = {
		{
			.name	= "-p option",
			.argv	= { target, "-p", pid_str, NULL},
		},
		{}, /* sentry */
	};
	const struct test *t;
	struct sigaction sa = {
		.sa_sigaction	= handler,
		.sa_flags	= SA_SIGINFO,
	};
	if (sigfillset(&sa.sa_mask) == -1) {
		perror("sigfillset");
		return 1;
	}
	if (sigaction(SIGUSR1, &sa, NULL) == -1) {
		perror("sigaction");
		return 1;
	}

	for (t = tests; t->name; t++) {
		pid_t pid;
		int ret;

		/* run daemon */
		pid = fork();
		if (pid == -1)
			perror("fork");
		else if (pid == 0) {
			ret = execv(target, t->argv);
			if (ret == -1)
				perror("execv");
			exit(EXIT_SUCCESS);
		}

		/* wait for the daemon to send signal back to us. */
		for (;;)
			if (pause() == -1) {
				if (errno != EINTR)
					perror("pause");
				break;
			}

		/* terminate the daemon */
		if ((ret = shutdown(&daemon_info)) != 0)
			return ret;
	}
	return 0;
}
