/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

static sig_atomic_t child_pid = -1;
static sig_atomic_t daemon_pid = -1;
static sig_atomic_t daemon_status = -1;

static void sigchld_action(int signo, siginfo_t *si, void *tcontext)
{
	fprintf(stderr, "sigchld from %d\n", si->si_pid);
	if (signo != SIGCHLD)
		return;
	/* we only handle the process generated signal */
	if (si->si_code != SI_QUEUE)
		return;
	if (si->si_pid == child_pid) {
		/* from the child */
		daemon_pid = si->si_int;
		printf("daemon[%d]\n", daemon_pid);
	} else if (si->si_pid == daemon_pid) {
		/* from the daemon */
		daemon_status = si->si_int;
		printf("daemon[%d]: status=%d\n", daemon_pid, daemon_status);
	}
}

static char *pidstring(pid_t pid)
{
	char buf[BUFSIZ];
	char *pidstr;
	int ret;

	ret = snprintf(buf, sizeof(buf), "%d", pid);
	if (ret < 0) {
		perror("snprintf");
		abort();
	}
	pidstr = strndup(buf, ret+1);
	if (pidstr == NULL) {
		perror("strdup");
		abort();
	}
	printf("%s\n", pidstr);
	return pidstr;
}

int main(void)
{
	char *const pidstr = pidstring(getpid());
	char *const target = realpath("./server", NULL);
	const struct test {
		const char	*const name;
		char		*const argv[6];
		int		want;
		int		daemonize;
		int		daemon_want;
	} *t, tests[] = {
		{
			.name		= "-h option",
			.argv		= {target, "-h", NULL},
			.want		= 0,
			.daemonize	= 0,
		},
		{
			.name		= "1sec timeout",
			.argv		= {target, "-t", "1", "--ppid", pidstr, NULL},
			.want		= 0,
			.daemonize	= 1,
			.daemon_want	= 0,
		},
		{}, /* sentry */
	};
	struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= sigchld_action,
	};
	int ret = -1;

	ret = sigemptyset(&sa.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		goto out;
	}
	for (t = tests; t->name; t++) {
		int status;
		pid_t pid;

		sa.sa_flags |= SA_RESTART;
		ret = sigaction(SIGCHLD, &sa, NULL);
		if (ret == -1) {
			perror("sigaction");
			break;
		}
		child_pid = pid = fork();
		if (pid == -1) {
			perror("fork");
			abort();
		} else if (pid == 0) {
			/* child */
			ret = execv(target, t->argv);
			if (ret == -1) {
				perror("execv");
				exit(EXIT_FAILURE);
			}
			/* not reachable */
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			abort();
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "%s: unexpected child signal(%s)\n",
				t->name, strsignal(WTERMSIG(status)));
			break;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "%s: unexpected child not terminated\n",
				t->name);
			break;
		}
		if (WEXITSTATUS(status) != t->want) {
			fprintf(stderr, "%s: unexpected exit status\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->want, WEXITSTATUS(status));
			break;
		}
		if (!t->daemonize) {
			ret = 0;
			continue;
		}
		if (daemon_pid == -1) {
			fprintf(stderr, "%s: cannot receive daemon PID\n",
				t->name);
			break;
		}
		fprintf(stderr, "daemon_pid[%d]\n", daemon_pid);
		sa.sa_flags &= ~SA_RESTART;
		ret = sigaction(SIGCHLD, &sa, NULL);
		if (ret == -1) {
			perror("sigaction");
			break;
		}
		/* wait for the daemon to exit */
		pause();
		if (daemon_status != t->daemon_want) {
			fprintf(stderr, "%s: unexpected daemon exit status\n\t- want: %d\n\t-  got: %d\n",
				t->name, t->daemon_want, daemon_status);
			break;
		}
		daemon_status = -1;
		daemon_pid = -1;
		child_pid = -1;
		ret = 0;
	}
out:
	if (target)
		free(target);
	if (pidstr)
		free(pidstr);
	if (ret)
		return 1;
	return 0;
}
