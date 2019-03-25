/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <mqueue.h>
#include <sys/types.h>
#include <sys/stat.h>
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

/* command handler client */
struct client {
	const struct process	*p;
	const char		*const mqpath;
	int			(*handle)(struct client *c, char *const argv[]);
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
	.c.p		= NULL,
	.c.mqpath	= "/somemsgq",
	.c.handle	= NULL,
	.delim		= " \t\n",
	.version	= "1.0.2",
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

static int is_print_prompt(const struct process *const p)
{
	return p->prompt[0] != '\0';
}

static void print_prompt(const struct process *const p)
{
	if (is_print_prompt(p))
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

static int client_handler(struct client *ctx, char *const argv[])
{
	int ret, status;
	pid_t pid;

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
	return 0;
}

static int client_pipe_handler(struct client *ctx, char *const argv[])
{
	int i, ret, status, in[2], out[2];
	char *ptr, buf[LINE_MAX];
	FILE *file[2];
	ssize_t len;
	pid_t pid;

	ret = pipe(in);
	if (ret == -1) {
		perror("pipe(in)");
		return -1;
	}
	ret = pipe(out);
	if (ret == -1) {
		perror("pipe(out)");
		return -1;
	}
	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid == 0) {
		char *const path = realpath("./sh", NULL);
		char *const argv[] = {path, "-p", "", NULL};
		if (path == NULL) {
			perror("realpath");
			exit(EXIT_FAILURE);
		}
		ret = dup2(in[0], STDIN_FILENO);
		if (ret == -1) {
			perror("dup(in)");
			exit(EXIT_FAILURE);
		}
		ret = close(in[1]);
		if (ret == -1) {
			perror("close(in)");
			exit(EXIT_FAILURE);
		}
		ret = dup2(out[1], STDOUT_FILENO);
		if (ret == -1) {
			perror("dup(out)");
			exit(EXIT_FAILURE);
		}
		ret = close(out[0]);
		if (ret == -1) {
			perror("close(out)");
			exit(EXIT_FAILURE);
		}
		ret = execv(path, argv);
		if (ret == -1) {
			perror("execv");
			exit(EXIT_FAILURE);
		}
		/* not reachable */
	}
	file[0] = fdopen(out[0], "r");
	if (file[0] == NULL) {
		perror("fdopen");
		goto err;
	}
	file[1] = fdopen(in[1], "w");
	if (file[1] == NULL) {
		perror("fdopen");
		goto err;
	}
	ret = close(in[0]);
	if (ret == -1) {
		perror("close(in)");
		goto err;
	}
	ret = close(out[1]);
	if (ret == -1) {
		perror("close(out)");
		goto err;
	}
	/* recreate the command line */
	len = sizeof(buf)-2;
	ptr = buf;
	i = 0;
	for (i = 0; argv[i]; i++) {
		int n = snprintf(ptr, len, "%s ", argv[i]);
		if (n < 0) {
			perror("snprintf");
			goto err;
		}
		len -= n;
		ptr += n;
	}
	*ptr++ = '\n';
	*ptr = '\0';
	if (fputs(buf, file[1]) == EOF) {
		perror("fputs");
		goto err;
	}
	ret = fclose(file[1]);
	if (ret == -1) {
		perror("fclose");
		goto err;
	}
	while (fgets(buf, sizeof(buf), file[0]))
		if (fputs(buf, stdout) == EOF)
			break;
	if (fclose(file[0])) {
		perror("fclose");
		goto err;
	}
	ret = waitpid(pid, &status, 0);
	if (ret == -1) {
		perror("waitpid");
		return -1;
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "child exit with signal(%s)\n",
			strsignal(WTERMSIG(status)));
		return -1;
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "child does not exit\n");
		return -1;
	}
	return WEXITSTATUS(status);
err:
	if (kill(pid, SIGTERM))
		perror("kill");
	if (waitpid(pid, &status, 0))
		perror("waitpid");
	return -1;
}

static int server_mq_handler(const struct process *restrict p, mqd_t mq, int fd)
{
	char *argv[ARG_MAX] = {NULL};
	char *save, buf[LINE_MAX];
	char *start = buf;
	int i, ret;

	ret = dup2(fd, STDOUT_FILENO);
	if (ret == -1){
		perror("dup2");
		goto out;
	}
	ret = mq_receive(mq, buf, sizeof(buf), NULL);
	if (ret == -1) {
		perror("mq_receive");
		goto out;
	}
	for (i = 0; i < LINE_MAX; i++) {
		argv[i] = strtok_r(start, p->delim, &save);
		if (argv[i] == NULL)
			break;
		start = NULL;
	}
	if (argv[0] == NULL)
		goto out;
	ret = execvp(argv[0], argv);
	if (ret == -1) {
		perror("execvp");
		goto out;
	}
	/* not reachable */
out:
	if (close(fd))
		perror("close");
	if (mq_close(mq))
		perror("mq_close");
	return ret;
}

static int client_mq_handler(struct client *ctx, char *const argv[])
{
	const struct mq_attr attr = {
		.mq_flags	= 0,
		.mq_maxmsg	= 10,
		.mq_msgsize	= LINE_MAX,
		.mq_curmsgs	= 0,
	};
	char *ptr, buf[LINE_MAX];
	int len = sizeof(buf);
	int ret, status;
	FILE *file = NULL;
	int out[2];
	pid_t pid;
	mqd_t mq;
	int i;

	mq = mq_open(ctx->mqpath, O_CREAT|O_RDWR, 0644, &attr);
	if (mq == -1) {
		perror("mq_open");
		return -1;
	}
	if (mq_unlink(ctx->mqpath)) {
		perror("mq_unlink");
		goto out;
	}
	ret = pipe(out);
	if (ret == -1) {
		perror("pipe");
		goto out;
	}
	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid == 0) {
		ret = close(out[0]);
		if (ret == -1) {
			perror("close");
			exit(EXIT_FAILURE);
		}
		ret = server_mq_handler(ctx->p, mq, out[1]);
		if (ret)
			exit(EXIT_FAILURE);
		exit(EXIT_SUCCESS);
	}
	ret = close(out[1]);
	if (ret == -1) {
		perror("close");
		goto err;
	}
	file = fdopen(out[0], "r");
	if (file == NULL) {
		perror("fdopen");
		goto err;
	}
	ptr = buf;
	for (i = 0; argv[i]; i++) {
		ret = snprintf(ptr, len, "%s ", argv[i]);
		if (ret < 0) {
			perror("snprintf");
			goto err;
		}
		ptr += ret;
		len -= ret;
	}
	*ptr = '\0';
	ret = mq_send(mq, buf, strlen(buf), 0);
	if (ret == -1) {
		perror("mq_send");
		goto err;
	}
	while (fgets(buf, sizeof(buf), file))
		if (fputs(buf, stdout) == EOF)
			break;
err:
	ret = waitpid(pid, &status, 0);
	if (ret == -1) {
		perror("waitpid");
		goto out;
	}
	ret = -1;
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "child exit with signal(%s)\n",
			strsignal(WTERMSIG(status)));
		goto out;
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "child did not exit\n");
		goto out;
	}
	ret = 0;
out:
	if (file)
		if (fclose(file))
			perror("fclose");
	if (mq_close(mq))
		perror("mq_close");
	return ret;
}

static int init_client(struct process *const p)
{
	switch (p->ipc) {
	case IPC_NONE:
		p->c.handle = client_handler;
		break;
	case IPC_PIPE:
		p->c.handle = client_pipe_handler;
		break;
	case IPC_MSGQ:
		p->c.handle = client_mq_handler;
		break;
	default:
		fprintf(stderr, "unknown ipc type: %d\n", p->ipc);
		return -1;
		break;
	}
	p->c.p = p;
	return 0;
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

/* shell/internal commands and the handlers */
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
		if (!strncasecmp(argv0, cmd->name, strlen(argv0)))
			return cmd;
		for (i = 0; cmd->alias[i]; i++)
			if (!strncasecmp(argv0, cmd->alias[i], strlen(argv0)))
				return cmd;
	}
	return NULL;
}

static int handle(struct client *ctx, char *cmdline)
{
	const struct process *const p = ctx->p;
	char *save, *start = cmdline;
	char *argv[ARG_MAX] = {NULL};
	const struct command *cmd;
	int i, ret;

	for (i = 0; i < ARG_MAX; i++) {
		argv[i] = strtok_r(start, p->delim, &save);
		if (!argv[i])
			break;
		start = NULL;
	}
	if (!argv[0])
		return 0;

	if ((cmd = parse_command(argv[0])))
		/* internal command handling */
		ret = (*cmd->handler)(i, argv);
	else
		/* external command handling */
		ret = (*ctx->handle)(ctx, argv);
	if (ret != 0)
		return ret;
	print_prompt(p);
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
			if (!strncasecmp(optarg, "none", strlen(optarg)))
				p->ipc = IPC_NONE;
			else if (!strncasecmp(optarg, "pipe", strlen(optarg)))
				p->ipc = IPC_PIPE;
			else if (!strncasecmp(optarg, "msgq", strlen(optarg)))
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
		if ((ret = handle(&p->c, cmd)))
			return 1;
	if (is_print_prompt(p))
		putchar('\n');

	return 0;
}
