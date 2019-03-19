/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "ls.h"

#ifndef ARG_MAX
#define ARG_MAX 1024
#endif /* ARG_MAX */

typedef enum ipc_type {
	IPC_NONE = 0,
	IPC_PIPE,
	IPC_MSGQ,
} ipc_t;

struct process;

struct client {
	int			(*handle)(const struct process *restrict p,
					  char *cmdline);
};

static struct process {
	unsigned		timeout;
	const char		*prompt;
	ipc_t			ipc;
	struct client		c;
	const char		*progname;
	const char		*delim;
	const char		*const version;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.timeout	= 30,
	.prompt		= "sh",
	.ipc		= IPC_NONE,
	.delim		= " \t\n",
	.version	= "1.0.1",
	.opts		= "t:p:i:h",
	.lopts		= {
		{"timeout",	required_argument,	NULL,	't'},
		{"prompt",	required_argument,	NULL,	'p'},
		{"ipc",		required_argument,	NULL,	'i'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *stream, int status)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(stream, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(stream,"\tspecify timeout in second (default %d)\n",
				p->timeout);
			break;
		case 'p':
			fprintf(stream, "\tspecify the prompt (default '%s$ ')\n",
				p->prompt);
			break;
		case 'i':
			fprintf(stream, "\tIPC type [none|pipe|msgq] (default: none)\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option", o->name);
			break;
		}
	}
	exit(status);
}

static void timeout_handler(int signo)
{
	if (signo != SIGALRM)
		return;
	printf("\n");
	exit(EXIT_SUCCESS);
}

static int init_timeout(unsigned timeout)
{
	signal(SIGALRM, timeout_handler);
	while (alarm(timeout))
		alarm(0);
	return 0;
}

static void io_handler(int signo)
{
	const struct process *const p = &process;
	if (signo != SIGIO)
		return;
	while (alarm(p->timeout))
		alarm(0);
}

static int init_io(int fd)
{
	int ret;

	/* setup the handler before owning the file descriptor
	 * to make sure we always receives the SIGIO. */
	signal(SIGIO, io_handler);
	ret = fcntl(fd, F_GETFL);
	if (ret == -1) {
		perror("fcntl(F_GETFL)");
		return -1;
	}
	ret = fcntl(fd, F_SETFL, ret|O_ASYNC);
	if (ret == -1) {
		perror("fcntl(F_SETFL)");
		return -1;
	}
	ret = fcntl(fd, F_SETOWN, getpid());
	if (ret == -1) {
		perror("fcntl(F_SETOWN)");
		return -1;
	}
	return 0;
}

static void print_prompt(const struct process *const p)
{
	printf("%s$ ", p->prompt);
}

static int exit_handler(int argc, char *const argv[])
{
	exit(EXIT_SUCCESS);
	return 0;
}

static int version_handler(int argc, char *const argv[])
{
	const struct process *const p = &process;
	printf("version %s\n", p->version);
	return 0;
}

/* shell command and the handler */
static const struct command {
	const char	*const name;
	const char	*const alias[2];
	int		(*handler)(int argc, char *const argv[]);
} cmds[] = {
	{
		.name		= "ls",
		.alias		= {NULL},
		.handler	= lsp_ls,
	},
	{
		.name		= "exit",
		.alias		= {"quit", NULL},
		.handler	= exit_handler,
	},
	{
		.name		= "version",
		.alias		= {NULL},
		.handler	= version_handler,
	},
	{}, /* sentry */
};

static const struct command *parse_command(char *argv0)
{
	const struct command *cmd;
	for (cmd = cmds; cmd->name; cmd++) {
		int i;
		if (!strncmp(argv0, cmd->name, strlen(argv0)))
			return cmd;
		for (i = 0; cmd->alias[i]; i++)
			if (!strncmp(argv0, cmd->alias[i], strlen(argv0)))
				return cmd;
	}
	return NULL;
}

static int handle(const struct process *restrict p, char *cmdline)
{
	char *save, *start = cmdline;
	char *argv[ARG_MAX] = {NULL};
	const struct command *cmd;
	int i, ret, status;
	pid_t pid;

	for (i = 0; i < ARG_MAX; i++) {
		argv[i] = strtok_r(start, p->delim, &save);
		if (!argv[i])
			break;
		start = NULL;
	}
	if (!argv[0])
		return 0;

	/* internal command handling */
	if ((cmd = parse_command(argv[0]))) {
		ret = (*cmd->handler)(i, argv);
		if (!ret)
			print_prompt(p);
		return ret;
	}

	/* external command handling */
	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid == 0) {
		ret = execvp(argv[0], argv);
		if (ret == -1) {
			perror("execvp");
			exit(EXIT_FAILURE);
		}
		/* not reachable */
	}
	ret = wait(&status);
	if (ret == -1) {
		perror("wait");
		return -1;
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "child exit with signal(%s)\n",
			strsignal(WTERMSIG(status)));
		return -1;
	}
	if (WIFEXITED(status) && WEXITSTATUS(status))
		return WEXITSTATUS(status);
	print_prompt(p);
	return 0;
}

static int init_client(struct process *const p)
{
	switch (p->ipc) {
	case IPC_NONE:
		p->c.handle = handle;
		return 0;
	default:
		fprintf(stderr, "unknown ipc type: %d\n", p->ipc);
		break;
	}
	return -1;
}

static int init(struct process *const p, int fd)
{
	int ret;

	ret = init_timeout(p->timeout);
	if (ret == -1)
		return ret;
	ret = init_io(fd);
	if (ret == -1)
		return ret;
	ret = init_client(p);
	if (ret == -1)
		return ret;
	return 0;
}

int main(int argc, char *const argv[])
{
	struct process *const p = &process;
	char *cmd, line[LINE_MAX];
	int opt, ret;

	p->progname = argv[0];
	while ((opt = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val == LONG_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = (unsigned)val;
			break;
		case 'p':
			p->prompt = optarg;
			break;
		case 'i':
			if (!strncmp(optarg, "none", strlen(optarg)))
				p->ipc = IPC_NONE;
			else if (!strncmp(optarg, "pipe", strlen(optarg)))
				p->ipc = IPC_PIPE;
			else if (!strncmp(optarg, "msgq", strlen(optarg)))
				p->ipc = IPC_MSGQ;
			else
				usage(p, stderr, EXIT_FAILURE);
			break;
		case 'h':
			usage(p, stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(p, stderr, EXIT_FAILURE);
			break;
		}
	}
	if (init(p, STDIN_FILENO))
		return 1;

	/* let's roll */
	print_prompt(p);
	while ((cmd = fgets(line, sizeof(line), stdin)))
		if ((ret = (*p->c.handle)(p, cmd)))
			return 1;
	putchar('\n');

	return 0;
}
