/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <signal.h>
#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#ifndef NR_OPEN
#define NR_OPEN 1024
#endif /* NR_OPEN */

struct client {
	struct server		*top;
	int			sd;
	struct sockaddr_in	addr;
	pthread_t		tid;
	pthread_cond_t		cond;
	pthread_mutex_t		lock;
};

static struct server {
	const struct parameter	*p;
	int			sd;
	int			cindex;
	struct client		*clients;
	sem_t			sem;
} server = {
	.sd	= -1,
	.cindex	= 0,
};

static const char *progname;
static struct parameter {
	unsigned	timeout;
	unsigned	backlog;
	int		daemon;
	int		single;
	int		pfamily;
	int		afamily;
	int		type;
	int		proto;
	int		port;
	struct client	*(*fetch)(struct server *);
	int		(*process)(struct client *);
	FILE		*output;
} param = {
	.timeout	= 30,
	.backlog	= 5,
	.daemon		= 0,
	.single		= 0,
	.pfamily	= PF_INET,
	.afamily	= AF_INET,
	.type		= SOCK_STREAM,
	.proto		= 0,
	.port		= 9999,
};
static const char *const opts = "t:b:dsh";
static const struct option lopts[] = {
	{"timeout",	required_argument,	NULL,	't'},
	{"backlog",	required_argument,	NULL,	'b'},
	{"daemon",	no_argument,		NULL,	'd'},
	{"single",	no_argument,		NULL,	's'},
	{"help",	no_argument,		NULL,	'h'},
	{NULL,		0,			NULL,	0},
};

static void usage(FILE *stream, int status)
{
	const struct parameter *p = &param;
	const struct option *o;

	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s:", o->val, o->name);
		switch (o->val) {
		case 't':
			fprintf(stream, "\tserver inactivity timeout (default: %us)\n",
				p->timeout);
			break;
		case 'b':
			fprintf(stream, "\tserver listen backlog (default: %d)\n",
				p->backlog);
			break;
		case 'd':
			fprintf(stream, "\tdaemonize the server\n");
			break;
		case 's':
			fprintf(stream, "\tsingle threaded server\n");
			break;
		case 'h':
			fprintf(stream, "\tdisplay this message and exit\n");
			break;
		default:
			fprintf(stream, "\t%s option\n", o->name);
			break;
		}
	}
	exit(status);
}

static int init_daemon(const struct parameter *restrict p)
{
	int i, ret, fd = -1;
	pid_t pid;

	if (!p->daemon)
		return 0;

	pid = fork();
	if (pid == -1) {
		perror("fork");
		return -1;
	} else if (pid)
		/* parent */
		exit(EXIT_SUCCESS);

	/* child */
	ret = setsid();
	if (ret == -1) {
		perror("setsid");
		goto err;
	}
	ret = chdir("/");
	if (ret == -1) {
		perror("chdir");
		goto err;
	}
	for (i = 0; i < NR_OPEN; i++)
		close(i);
	fd = open("/dev/null", O_RDWR);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	ret = dup(fd);
	if (ret == -1) {
		perror("dup");
		goto err;
	}
	ret = dup(fd);
	if (ret == -1) {
		perror("dup");
		goto err;
	}
	/* daemon now */
	return 0;
err:
	if (fd != -1)
		if (close(fd))
			perror("close(/dev/null)");
	return ret;
}

static void timeout_action(int signo, siginfo_t *si, void *tcontext)
{
	if (signo != SIGALRM)
		return;
	exit(EXIT_SUCCESS);
}

static int init_signal(void)
{
	const struct parameter *p = &param;
	struct sigaction sa = {
		.sa_flags	= SA_SIGINFO,
		.sa_sigaction	= timeout_action,
	};
	int ret;

	/* nothing to do in case of no timeout */
	if (!p->timeout)
		return 0;

	/* for SIGALRM */
	ret = sigemptyset(&sa.sa_mask);
	if (ret == -1) {
		perror("sigemptyset");
		return ret;
	}
	ret = sigaction(SIGALRM, &sa, NULL);
	if (ret == -1) {
		perror("sigaction(SIGALRM)");
		return ret;
	}
	return 0;
}

static int init_timer(const struct parameter *restrict p)
{
	unsigned timeout_remain = p->timeout;

	/* set the timeout alarm */
	alarm(0);
	while (timeout_remain)
		timeout_remain = alarm(timeout_remain);

	return 0;
}

static int reset_timer(const struct parameter *restrict p)
{
	sigset_t mask;
	int ret;

	if (!p->timeout)
		return 1;

	ret = sigemptyset(&mask);
	if (ret == -1) {
		perror("sigemptymask");
		return -1;
	}
	ret = sigaddset(&mask, SIGALRM);
	if (ret == -1) {
		perror("sigaddset");
		return -1;
	}
	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	ret = init_timer(p);
	if (ret == -1)
		return -1;
	ret = sigprocmask(SIG_UNBLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	return 0;
}

static int init_socket(const struct parameter *restrict p)
{
	struct sockaddr_in sin;
	int sd, ret, opt;

	sd = socket(p->pfamily, p->type, p->proto);
	if (sd == -1) {
		perror("socket");
		ret = -1;
		goto err;
	}
	opt = 1;
	ret = setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	if (ret == -1) {
		perror("setsockopt(SO_REUSEADDR)");
		goto err;
	}
	sin.sin_family = p->afamily;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(p->port);
	ret = bind(sd, (struct sockaddr *)&sin, sizeof(sin));
	if (ret == -1) {
		perror("bind");
		goto err;
	}
	ret = listen(sd, p->backlog);
	if (ret == -1) {
		perror("listen");
		goto err;
	}
	return sd;
err:
	if (sd != -1)
		if (close(sd))
			ret = -1;
	return ret;
}

static void *worker(void *arg);

static int init_server(const struct parameter *restrict p)
{
	struct server *s = &server;
	struct client *c;
	int i, ret;

	memset(s, 0, sizeof(struct server));
	c = calloc(p->backlog, sizeof(struct client));
	if (c == NULL) {
		perror("calloc");
		return -1;
	}
	s->cindex = 0;
	s->clients = c;
	for (i = 0; i < p->backlog; i++) {
		memset(&c->addr, 0, sizeof(c->addr));
		c->top = s;
		c->sd = -1;
		c++;
	}
	ret = init_socket(p);
	if (ret == -1)
		goto err;
	s->sd = ret;
	s->p = p;
	if (p->single)
		/* single threaded */
		return 0;

	/* multithreaded */
	ret = sem_init(&s->sem, 0, p->backlog);
	if (ret == -1) {
		perror("sem_init");
		goto err;
	}
	c = s->clients;
	for (i = 0; i < p->backlog; i++) {
		ret = pthread_mutex_init(&c->lock, NULL);
		if (ret) {
			errno = ret;
			perror("pthread_mutex_init");
			goto err;
		}
		ret = pthread_cond_init(&c->cond, NULL);
		if (ret) {
			errno = ret;
			perror("pthread_cond_init");
			goto err;
		}
		ret = pthread_create(&c->tid, NULL, worker, c);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			goto err;
		}
		c++;
	}
	return 0;
err:
	if (s->clients)
		free(s->clients);
	return -1;
}

static int init(const struct parameter *restrict p)
{
	int ret;

	ret = init_daemon(p);
	if (ret == -1)
		return ret;
	ret = init_signal();
	if (ret == -1)
		return ret;
	ret = init_timer(p);
	if (ret == -1)
		return ret;
	return init_server(p);
}

static void dump(FILE *s, const unsigned char *restrict buf, size_t len)
{
	int i, j, width = 16;

	for (i = 0; i < len; i++) {
		fprintf(s, "%02x ", buf[i]);
		/* check if we are done for this line */
		if (i%width == (width-1) || i+1 == len) {
			/* fill the gap */
			for (j = (i%width); j < (width-1); j++)
				fprintf(s, "   ");
			/* ascii print */
			fprintf(s, "| ");
			for (j = i-(i%width); j <= i; j++)
				fprintf(s, "%c", isprint(buf[j]) ? buf[j] : '.');
			fprintf(s, "\n");
		}
	}
}

static struct client *fetch(struct server *s)
{
	const struct parameter *const p = s->p;
	char buf[BUFSIZ];
	struct client *c;
	socklen_t len;
	int ret;

	c = &s->clients[s->cindex];
	len = sizeof(c->addr);
again:
	ret = accept(s->sd, (struct sockaddr *)&c->addr, &len);
	if (ret == -1) {
		/* canceled */
		if (errno == EINTR)
			return NULL;
		perror("accept");
		goto again;
	}
	c->sd = ret;
	fprintf(p->output, "from %s:%d\n",
		inet_ntop(p->afamily, &c->addr.sin_addr, buf, sizeof(buf)),
		ntohs(c->addr.sin_port));
	/* for the next fetch */
	s->cindex = (s->cindex+1)%p->backlog;
	return c;
}

static int process(struct client *c)
{
	const struct server *s = c->top;
	const struct parameter *const p = s->p;
	char buf[BUFSIZ];
	int ret;

	snprintf(buf, sizeof(buf), "Hello, World!\n");
	ret = send(c->sd, buf, strlen(buf), 0);
	if (ret == -1) {
		perror("send");
		goto out;
	}
	ret = recv(c->sd, buf, sizeof(buf), 0);
	while (ret > 0) {
		reset_timer(p);
		dump(p->output, (unsigned char *)buf, ret);
		if (ret == 1 && buf[0] == 4) {
			/* EOT, End Of Transmission */
			ret = 0;
			break;
		}
		ret = recv(c->sd, buf, sizeof(buf), 0);
	}
out:
	if (close(c->sd))
		perror("close");
	c->sd = -1;
	return ret;
}

static void *worker(void *arg)
{
	struct client *ctx = arg;
	struct server *s = ctx->top;
	int ret;

	while (1) {
		ret = pthread_mutex_lock(&ctx->lock);
		if (ret) {
			if (ret == EINTR) {
				ret = 0;
				goto out;
			}
			errno = ret;
			perror("pthread_mutex_lock");
			continue;
		}
		while (ctx->sd == -1) {
			ret = pthread_cond_wait(&ctx->cond, &ctx->lock);
			if (ret) {
				if (ret == EINTR) {
					ret = 0;
					goto out;
				}
				errno = ret;
				perror("pthread_cond_wait");
				continue;
			}
		}
		if (process(ctx) == -1)
			; /* ignore for now... */

		ret = pthread_mutex_unlock(&ctx->lock);
		if (ret) {
			if (ret == EINTR) {
				ret = 0;
				goto out;
			}
		}
		ret = sem_post(&s->sem);
		if (ret == -1) {
			if (ret == EINTR) {
				ret = 0;
				goto out;
			}
			perror("sem_post");
		}
	}
out:
	if (ret)
		return (void *)EXIT_FAILURE;
	return NULL;
}

static struct client *fetch_multi(struct server *s)
{
	const struct parameter *const p = s->p;
	char buf[BUFSIZ];
	struct client *c;
	socklen_t len;
	int i, ret;

fetch:
	while ((ret = sem_wait(&s->sem)))
		if (errno == EINTR)
			return NULL;
	for (i = 0; i < p->backlog; i++) {
		c = &s->clients[(s->cindex+i)%p->backlog];
		ret = pthread_mutex_trylock(&c->lock);
		if (ret)
			continue;
		break;
	}
	if (i == p->backlog)
		goto fetch;
accept:
	len = sizeof(c->addr);
	ret = accept(s->sd, (struct sockaddr *)&c->addr, &len);
	if (ret == -1) {
		/* canceled */
		if (errno == EINTR) {
			if ((ret = pthread_mutex_unlock(&c->lock))) {
				errno = ret;
				perror("pthread_mutex_unlock");
			}
			return NULL;
		}
		perror("accept");
		goto accept;
	}
	c->sd = ret;
	fprintf(p->output, "from %s:%d\n",
		inet_ntop(p->afamily, &c->addr.sin_addr, buf, sizeof(buf)),
		ntohs(c->addr.sin_port));
	/* for the next fetch */
	s->cindex = (s->cindex+1)%p->backlog;
	return c;
}

static int process_multi(struct client *ctx)
{
	int ret;

	ret = pthread_mutex_unlock(&ctx->lock);
	if (ret) {
		errno = ret;
		perror("pthread_mutex_unlock");
		return -1;
	}
	ret = pthread_cond_signal(&ctx->cond);
	if (ret) {
		errno = ret;
		perror("pthread_cond_signal");
		return -1;
	}
	return 0;
}

static void term(struct server *ctx)
{
	const struct parameter *const p = ctx->p;
	struct client *c;
	int i, ret;

	c = ctx->clients;
	for (i = 0; i < p->backlog; i++) {
		ret = pthread_cancel(c->tid);
		if (ret) {
			errno = ret;
			perror("pthread_cancel");
		}
	}
	ret = sem_destroy(&ctx->sem);
	if (ret == -1)
		perror("sem_destroy");
	if (ctx->sd != -1)
		if (close(ctx->sd))
			perror("close");
}

int main(int argc, char *argv[])
{
	struct parameter *p = &param;
	struct server *s = &server;
	struct client *c;
	int ret, opt;

	progname = argv[0];
	p->output = stdout;
	p->fetch = fetch_multi;
	p->process = process_multi;
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		long val;
		switch (opt) {
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val >= LONG_MAX)
				usage(stderr, EXIT_FAILURE);
			p->timeout = val;
			break;
		case 'b':
			val = strtol(optarg, NULL, 10);
			if (val <= 0 || val >= LONG_MAX)
				usage(stderr, EXIT_FAILURE);
			p->backlog = val;
			break;
		case 'd':
			p->daemon = 1;
			break;
		case 's':
			p->single = 0;
			p->fetch = fetch;
			p->process = process;
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE);
			break;
		}
	}
	ret = init(p);
	if (ret == -1)
		goto out;

	/* light the fire */
	while ((c = (*p->fetch)(s)))
	       if ((ret = (*p->process)(c)))
		       goto out;
out:
	term(s);
	if (ret)
		return 1;
	return 0;
}
