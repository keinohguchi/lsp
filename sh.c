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
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>

#include "ls.h"

#ifndef ARG_MAX
#define ARG_MAX 1024
#endif /* ARG_MAX */

typedef enum ipc_type {
	IPC_NONE = 0,
	IPC_PIPE,
	IPC_MSGQ,
} ipc_t;

static struct process {
	unsigned		timeout;
	const char		*prompt;
	ipc_t			ipc;
	const char		*progname;
	const char		*delim;
	const char		*const mqpath;
	const char		*const sempath;
	const char		*const shmpath;
	mqd_t			mq;
	struct mq_attr		mq_attr;
	pid_t			mq_pid;
	sem_t			*sem;
	int			shm;
	off_t			shmsize;
	int			(*handle)(const struct process *restrict p,
					  char *const argv[]);
	const char		*const version;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.timeout	= 30000,
	.prompt		= "sh",
	.ipc		= IPC_NONE,
	.mqpath		= "/somemq",
	.sempath	= "/somesem",
	.shmpath	= "/someshm",
	.mq		= -1,
	.mq_attr	= {
		.mq_flags	= 0,
		.mq_maxmsg	= 10,
		.mq_msgsize	= LINE_MAX,
		.mq_curmsgs	= 0,
	},
	.mq_pid		= -1,
	.sem		= NULL,
	.shm		= -1,
	.delim		= " \t\n",
	.handle		= NULL,
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

/* message passed over the shared memory */
struct message {
	u_int32_t	len;
	char		data[];
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
			fprintf(stream,"\tspecify timeout in millisecond (default %u)\n",
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

static void term(const struct process *restrict p);

static void timeout_handler(int signo)
{
	const struct process *const p = &process;
	if (signo != SIGALRM)
		return;
	printf("\n");
	term(p);
	exit(EXIT_SUCCESS);
}

static int init_timer(unsigned timeout)
{
	const struct itimerval it = {
		.it_value	= {timeout/1000, (timeout%1000)*1000},
		.it_interval	= {0, 0},
	};
	int ret;

	ret = setitimer(ITIMER_REAL, &it, NULL);
	if (ret == -1)
		perror("setitimer");
	return ret;
}

static int init_timeout(unsigned timeout)
{
	signal(SIGALRM, timeout_handler);
	return init_timer(timeout);
}

static void io_handler(int signo)
{
	const struct process *const p = &process;
	const struct itimerval zero = {
		.it_value	= {0, 0},
		.it_interval	= {0, 0},
	};
	if (signo != SIGIO)
		return;
	setitimer(ITIMER_REAL, &zero, NULL);
	init_timer(p->timeout);
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

static int handler(const struct process *restrict p, char *const argv[])
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

static int pipe_handler(const struct process *restrict p, char *const argv[])
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

static int mq_server(const struct process *restrict p)
{
	char *ptr, buf[LINE_MAX];
	size_t len, total, remain;
	struct message *msg;
	FILE *file;
	int ret, status;
	pid_t pid;
	int out[2];

	while (1) {
		file = NULL;
		msg = NULL;
		ret = mq_receive(p->mq, buf, sizeof(buf), NULL);
		if (ret == -1) {
			perror("mq_receive");
			break;
		}
		ret = pipe(out);
		if (ret == -1) {
			perror("pipe");
			break;
		}
		pid = fork();
		if (pid == -1) {
			perror("fork");
			break;
		} else if (pid == 0) {
			char *argv[ARG_MAX] = {NULL};
			char *save, *start = buf;
			int i, ret;

			if (close(out[0])) {
				perror("close");
				exit(EXIT_FAILURE);
			}
			ret = dup2(out[1], STDOUT_FILENO);
			if (ret == -1) {
				perror("dup2");
				exit(EXIT_FAILURE);
			}
			for (i = 0; i < LINE_MAX; i++) {
				argv[i] = strtok_r(start, p->delim, &save);
				if (argv[i] == NULL)
					break;
				start = NULL;
			}
			ret = execvp(argv[0], argv);
			if (ret == -1) {
				perror("execvp");
				exit(EXIT_FAILURE);
			}
			/* not reached */
		}
		ret = close(out[1]);
		if (ret == -1) {
			perror("close");
			break;
		}
		file = fdopen(out[0], "r");
		if (file == NULL) {
			perror("fdopen");
			break;
		}
		msg = mmap(NULL, p->shmsize, PROT_WRITE, MAP_SHARED, p->shm, 0);
		if (msg == NULL) {
			perror("mmap");
			break;
		}
		remain = p->shmsize-sizeof(msg->len);
		total = msg->len = 0;
		ptr = (char *)msg->data;
		while ((len = fread(ptr, 1, remain, file))) {
			remain -= len;
			total += len;
			ptr += len;
		}
		if (ferror(file)) {
			perror("fread");
			ret = -1;
			break;
		}
		msg->len = total;
		ret = sem_post(p->sem);
		if (ret == -1) {
			perror("sem_post");
			break;
		}
		ret = waitpid(pid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			break;
		}
		ret = -1;
		if (WIFSIGNALED(status)) {
			fprintf(stderr, "child exit with signal(%s)\n",
				strsignal(WTERMSIG(status)));
			break;
		}
		if (!WIFEXITED(status)) {
			fprintf(stderr, "child does not exit\n");
			break;
		}
		ret = WEXITSTATUS(status);
		if (ret != 0) {
			fprintf(stderr, "child exit with error status(%d)\n",
				WEXITSTATUS(status));
			/* ignore the error */
		}
	}
	if (msg)
		if (munmap(msg, p->shmsize))
		    perror("munmap");
	if (file)
		if (fclose(file))
			perror("fclose");
	if (mq_close(p->mq))
		perror("mq_close");
	if (sem_close(p->sem))
		perror("sem_close");
	return ret;
}

static int mq_handler(const struct process *restrict p, char *const argv[])
{
	struct message *msg = NULL;
	char *ptr, buf[LINE_MAX];
	size_t len = sizeof(buf);
	size_t n, remain;
	int i, ret;

	ptr = buf;
	for (i = 0; argv[i]; i++) {
		ret = snprintf(ptr, len, "%s ", argv[i]);
		if (ret < 0) {
			perror("snprintf");
			goto out;
		}
		ptr += ret;
		len -= ret;
	}
	*ptr = '\0';
	ret = mq_send(p->mq, buf, strlen(buf)+1, 0); /* null terminater */
	if (ret == -1) {
		perror("mq_send");
		goto out;
	}
	ret = sem_wait(p->sem);
	if (ret == -1) {
		perror("sem_wait");
		goto out;
	}
	msg = mmap(NULL, p->shmsize, PROT_READ, MAP_SHARED, p->shm, 0);
	if (msg == NULL) {
		perror("mmap");
		ret = -1;
		goto out;
	}
	ptr = msg->data;
	for (remain = msg->len; remain > 0; remain -= n) {
		n = write(STDOUT_FILENO, ptr, remain);
		if (n == -1) {
			perror("write");
			ret = -1;
			goto out;
		}
	}
out:
	if (msg)
		if (munmap(msg, sizeof(u_int32_t)))
			perror("munmap");
	return ret;
}

static int init_mq_handler(struct process *const p)
{
	pid_t pid;
	int ret;

	/* semaphore for the synchronization */
	p->sem = sem_open(p->sempath, O_CREAT|O_RDWR|O_EXCL, 0600, 0);
	if (p->sem == NULL) {
		perror("sem_open");
		return -1;
	}
	ret = sem_unlink(p->sempath);
	if (ret == -1) {
		perror("sem_unlink");
		goto err;
	}
	/* mqueue for the command passing */
	ret = -1;
	p->mq = mq_open(p->mqpath, O_CREAT|O_RDWR|O_EXCL, 0600, &p->mq_attr);
	if (p->mq == -1) {
		perror("mq_open");
		goto err;
	}
	ret = mq_unlink(p->mqpath);
	if (ret == -1) {
		perror("mq_unlink");
		goto err;
	}
	/* shared memory for the command response */
	ret = shm_open(p->shmpath, O_CREAT|O_RDWR|O_EXCL, 0600);
	if (ret == -1) {
		perror("shm_open");
		goto err;
	}
	p->shm = ret;
	ret = shm_unlink(p->shmpath);
	if (ret == -1) {
		perror("shm_unlink");
		goto err;
	}
	/* limit the shared memory to the 4 x page size */
	ret = sysconf(_SC_PAGESIZE);
	if (ret == -1) {
		perror("sysconf");
		goto err;
	}
	p->shmsize = ret*4;
	ret = ftruncate(p->shm, p->shmsize);
	if (ret == -1) {
		perror("ftruncate");
		goto err;
	}
	pid = fork();
	if (pid == -1) {
		perror("fork");
		goto err;
	} else if (pid == 0) {
		ret = mq_server(p);
		if (ret == -1)
			exit(EXIT_FAILURE);
		exit(EXIT_SUCCESS);
	}
	p->handle = mq_handler;
	return 0;
err:
	if (p->shm != -1) {
		if (close(p->shm))
			perror("close");
		if (shm_unlink(p->shmpath))
			perror("shm_unlink");
	}
	if (p->mq != -1) {
		if (mq_close(p->mq))
			perror("mq_close");
		if (mq_unlink(p->mqpath))
			perror("mq_unlink");
	}
	if (p->sem) {
		if (sem_close(p->sem))
			perror("sem_close");
		if (sem_unlink(p->sempath))
			perror("sem_unlink");
	}
	return ret;
}

static int init_handler(struct process *const p)
{
	switch (p->ipc) {
	case IPC_NONE:
		p->handle = handler;
		break;
	case IPC_PIPE:
		p->handle = pipe_handler;
		break;
	case IPC_MSGQ:
		if (init_mq_handler(p) == -1)
			return -1;
		break;
	default:
		fprintf(stderr, "unknown ipc type: %d\n", p->ipc);
		return -1;
		break;
	}
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
	ret = init_handler(p);
	if (ret == -1)
		return ret;
	return 0;
}

static void term(const struct process *restrict p)
{
	int ret, status;

	if (p->shm != -1)
		if (close(p->shm))
			perror("close");
	if (p->mq != -1)
		if (mq_close(p->mq))
			perror("mq_close");
	if (p->sem)
		if (sem_close(p->sem))
			perror("sem_close");
	if (p->mq_pid == -1)
		return;

	/* wait for the server to terminate */
	ret = waitpid(p->mq_pid, &status, 0);
	if (ret == -1) {
		perror("waitpid");
		return;
	}
	if (WIFSIGNALED(status)) {
		fprintf(stderr, "child exit with signal(%s)\n",
			strsignal(WTERMSIG(status)));
		return;
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "child does not exit\n");
		return;
	}
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

static int handle(const struct process *restrict p, char *cmdline)
{
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
		ret = (*p->handle)(p, argv);
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
		if ((ret = handle(p, cmd)))
			goto out;
	if (is_print_prompt(p))
		putchar('\n');
out:
	term(p);
	if (ret)
		return 1;
	return 0;
}
