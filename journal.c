/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <poll.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/timerfd.h>
#include <sys/signalfd.h>
#include <systemd/sd-daemon.h>
#include <systemd/sd-journal.h>

struct context {
	const struct process	*p;
	struct pollfd		fds[3];
	int			nfds;
	sd_journal		*jd;
	char			*cursor;
	size_t			cursor_len;
};

static struct process {
	struct context		journal[1];	/* single context */
	const char		*unit;
	const char		*cursor_file;
	int			max_entry;
	int			interval;
	int			timeout;
	const char		*progname;
	const char		*const opts;
	const struct option	lopts[];
} process = {
	.journal[0].jd		= NULL,
	.journal[0].cursor	= NULL,
	.journal[0].cursor_len	= 0,
	.journal[0].fds[0].fd	= -1,
	.journal[0].fds[1].fd	= -1,
	.journal[0].fds[2].fd	= -1,
	.journal[0].nfds	= sizeof(process.journal[0].fds)/sizeof(struct pollfd),
	.unit			= NULL,
	.cursor_file		= NULL,
	.max_entry		= 0,
	.interval		= 0,
	.timeout		= 0,
	.progname		= NULL,
	.opts			= "u:f:m:i:t:h",
	.lopts			= {
		{"unit",	required_argument,	NULL,	'u'},
		{"cursor_file",	required_argument,	NULL,	'f'},
		{"max_entry",	required_argument,	NULL,	'm'},
		{"interval",	required_argument,	NULL,	'i'},
		{"timeout",	required_argument,	NULL,	't'},
		{"help",	no_argument,		NULL,	'h'},
		{NULL, 0, NULL, 0},
	},
};

static void usage(const struct process *restrict p, FILE *s, int status)
{
	const struct option *o;
	fprintf(s, "usage: %s [-%s]\n", p->progname, p->opts);
	fprintf(s, "options:\n");
	for (o = p->lopts; o->name; o++) {
		fprintf(s, "\t-%c,--%s", o->val, o->name);
		switch (o->val) {
		case 'u':
			fprintf(s, "\t\tShow logs from the specified service unit\n");
			break;
		case 'f':
			fprintf(s, "\tPersistent journal cursor file (default: none)\n");
			break;
		case 'm':
			fprintf(s, "\t\tMaximum query entry limit for each invocation (default: none)\n");
			break;
		case 'i':
			fprintf(s, "\t\tInterval in millisecond (default: none)\n");
			break;
		case 't':
			fprintf(s, "\t\tTimeout in millisecond (default: none)\n");
			break;
		case 'h':
			fprintf(s, "\t\tDisplay this message and exit\n");
			break;
		default:
			fprintf(s, "\t\t%s option", o->name);
			break;
		}
	}
	exit(status);
}

static int init_cursor(struct process *p)
{
	struct context *ctx = p->journal;
	int ret, fd = -1;
	struct stat st;
	void *cursor;
	size_t len;

	ctx->cursor_len = 0;
	ctx->cursor = NULL;
	if (p->cursor_file == NULL)
		return 0;

	len = sysconf(_SC_LINE_MAX);
	if (len == -1) {
		perror("sysconf(_SC_LINE_MAX)");
		return -1;
	}
	fd = open(p->cursor_file, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
	if (fd == -1) {
		perror("open");
		return -1;
	}
	ret = fstat(fd, &st);
	if (ret == -1) {
		perror("fstat");
		goto err;
	}
	if (st.st_size < len) {
		ret = ftruncate(fd, len);
		if (ret == -1) {
			perror("ftruncate");
			goto err;
		}
	}
	cursor = mmap(NULL, len, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
	if (cursor == NULL) {
		perror("mmap");
		goto err;
	}
	if (close(fd)) {
		if (munmap(cursor, len))
			perror("munmap");
		perror("close");
		return -1;
	}
	ctx->cursor_len = len;
	ctx->cursor = cursor;
	return 0;
err:
	if (fd != -1)
		if (close(fd))
			perror("close");
	return -1;
}

static int init_journal(struct process *p)
{
	struct context *ctx = p->journal;
	int ret;

	ret = sd_journal_open(&ctx->jd, SD_JOURNAL_LOCAL_ONLY);
	if (ret < 0) {
		errno = -ret;
		perror("sd_journal_open");
		return -1;
	}
	if (p->unit) {
		char buf[LINE_MAX];
		snprintf(buf, sizeof(buf), "_SYSTEMD_UNIT=%s.service", p->unit);
		ret = sd_journal_add_match(ctx->jd, buf, 0);
		if (ret < 0) {
			errno = -ret;
			perror("sd_journal_add_match");
			goto err;
		}
	}
	if (ctx->cursor != NULL && ctx->cursor[0] != '\0') {
		ret = sd_journal_seek_cursor(ctx->jd, ctx->cursor);
		if (ret < 0) {
			errno = -ret;
			perror("sd_journal_seek_cursor");
			goto err;
		}
		/* Check if we find the previous position */
		ret = sd_journal_test_cursor(ctx->jd, ctx->cursor);
		if (ret) {
			/* advance the entry one in case we find it
			 * to avoid the duplicate log */
			ret = sd_journal_next(ctx->jd);
			if (ret < 0) {
				errno = -ret;
				perror("sd_journal_next");
				goto err;
			}
		}
	} else {
		ret = sd_journal_seek_head(ctx->jd);
		if (ret < 0) {
			errno = -ret;
			perror("sd_journal_seek_head");
			goto err;
		}
	}
	return 0;
err:
	if (ctx->jd)
		sd_journal_close(ctx->jd);
	return -1;
}

static int init_signal(const struct process *restrict p)
{
	sigset_t mask;
	int ret;

	ret = sigemptyset(&mask);
	if (ret == -1) {
		perror("sigemptyset");
		return -1;
	}
	ret = sigaddset(&mask, SIGTERM);
	if (ret == -1) {
		perror("sigaddset(SIGTERM)");
		return -1;
	}
	ret = sigaddset(&mask, SIGINT);
	if (ret == -1) {
		perror("sigaddset(SIGINT)");
		return -1;
	}
	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	return signalfd(-1, &mask, SFD_CLOEXEC);
}

static int init_restart(const struct process *restrict p)
{
	sigset_t mask;
	int ret;

	ret = sigemptyset(&mask);
	if (ret == -1) {
		perror("sigemptyset");
		return -1;
	}
	ret = sigaddset(&mask, SIGHUP);
	if (ret == -1) {
		perror("sigaddset(SIGHUP)");
		return -1;
	}
	ret = sigaddset(&mask, SIGABRT);
	if (ret == -1) {
		perror("sigaddset(SIGABRT)");
		return -1;
	}
	ret = sigprocmask(SIG_BLOCK, &mask, NULL);
	if (ret == -1) {
		perror("sigprocmask");
		return -1;
	}
	return signalfd(-1, &mask, SFD_CLOEXEC);
}

static int init_timer(const struct process *restrict p)
{
	const struct itimerspec ts = {
		.it_interval.tv_sec	= 0,
		.it_interval.tv_nsec	= 0,
		.it_value.tv_sec	= p->timeout/1000,
		.it_value.tv_nsec	= p->timeout%1000*1000*1000,
	};
	int fd, ret;

	fd = timerfd_create(CLOCK_REALTIME, TFD_CLOEXEC);
	if (fd == -1) {
		perror("timerfd_create");
		return -1;
	}
	ret = timerfd_settime(fd, 0, &ts, NULL);
	if (ret == -1) {
		perror("timerfd_settime");
		return -1;
	}
	return fd;
}

static int init(struct process *p)
{
	struct context *ctx = p->journal;
	int ret, fd;

	ret = init_cursor(p);
	if (ret == -1)
		return -1;
	ret = init_journal(p);
	if (ret == -1)
		goto err;
	fd = init_signal(p);
	if (fd == -1)
		goto err;
	ctx->fds[0].fd		= fd;
	ctx->fds[0].events	= POLLIN;
	fd = init_restart(p);
	if (fd == -1)
		goto err;
	ctx->fds[1].fd		= fd;
	ctx->fds[1].events	= POLLIN;
	fd = init_timer(p);
	if (fd == -1)
		goto err;
	ctx->fds[2].fd		= fd;
	ctx->fds[2].events	= POLLIN;
	ctx->p = p;
	return 0;
err:
	if (ctx->cursor)
		if (munmap(ctx->cursor, ctx->cursor_len))
			perror("munmap");
	return -1;
}

static void sigterm(const struct process *restrict p)
{
	const struct context *ctx = p->journal;
	if (ctx->cursor)
		if (munmap(ctx->cursor, ctx->cursor_len))
			perror("munmap");
	if (p->cursor_file)
		free((void *)p->cursor_file);
	if (p->unit)
		free((void *)p->unit);
}

static void term(const struct process *restrict p)
{
	const struct context *ctx = p->journal;
	int i;

	for (i = 0; i < ctx->nfds; i++)
		if (ctx->fds[i].fd != -1)
			if (close(ctx->fds[i].fd))
				perror("close");
	if (ctx->jd)
		sd_journal_close(ctx->jd);
	sigterm(p);
}

static int fetch(struct context *ctx)
{
	const struct process *const p = ctx->p;
	int ret;

	ret = poll(ctx->fds, ctx->nfds, p->interval);
	if (ret == -1) {
		perror("poll");
		return -1;
	} else if (ret == 0)
		return 0;
	term(p);
	if (ctx->fds[1].revents&POLLIN)
		/* restart */
		exit(EXIT_SUCCESS);
	if (ctx->fds[2].revents&POLLIN)
		/* timed out */
		exit(EXIT_SUCCESS);
	/* abnormal termination... */
	exit(EXIT_FAILURE);
}

static int exec(struct context *ctx)
{
	const struct process *const p = ctx->p;
	const char *field = "MESSAGE";
	size_t flen = strlen(field)+1; /* include trailing '=' */
	const char *data;
	char *cursor;
	size_t len;
	int i, ret;

	for (i = 0; p->max_entry == 0 || i < p->max_entry; i++) {
		ret = sd_journal_next(ctx->jd);
		if (ret == 0)
			break;
		else if (ret < 0) {
			errno = -ret;
			perror("sd_journal_next");
			break;
		}
		ret = sd_journal_get_data(ctx->jd, field, (const void **)&data, &len);
		if (ret < 0) {
			errno = -ret;
			perror("sd_journal_get_data");
			break;
		}
		/* Strip the field prefix */
		if (len <= flen)
			continue;
		printf("%*s\n", (int)(len-flen), data+flen);
	}
	if (i) {
		/* flush the buffer when there is an output */
		ret = fflush(stdout);
		if (ret == -1) {
			perror("fflush");
			return -1;
		}
		/* and let the systemd know we processed logs */
		ret = sd_notify(0, "WATCHDOG=1");
		if (ret < 0) {
			errno = ret;
			perror("sd_notify");
			return -1;
		}
	}
	ret = sd_journal_get_cursor(ctx->jd, &cursor);
	if (ret < 0) {
		errno = -ret;
		perror("sd_journal_get_cursor");
		return -1;
	}
	strncpy(ctx->cursor, cursor, ctx->cursor_len);
	free(cursor);
	return i;
}

int main(int argc, char *const argv[])
{
	struct process *p = &process;
	struct context *ctx;
	int ret, o;

	p->progname = argv[0];
	optind = 0;
	while ((o = getopt_long(argc, argv, p->opts, p->lopts, NULL)) != -1) {
		long val;
		switch (o) {
		case 'u':
			p->unit = strdup(optarg);
			if (p->unit == NULL) {
				perror("strdup");
				usage(p, stderr, EXIT_FAILURE);
			}
			break;
		case 'f':
			p->cursor_file = strdup(optarg);
			if (p->cursor_file == NULL) {
				perror("strdup");
				usage(p, stderr, EXIT_FAILURE);
			}
			break;
		case 'm':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val > INT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->max_entry = val;
			break;
		case 'i':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val > INT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->interval = val;
			break;
		case 't':
			val = strtol(optarg, NULL, 10);
			if (val < 0 || val > INT_MAX)
				usage(p, stderr, EXIT_FAILURE);
			p->timeout = val;
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
	ret = init(p);
	if (ret == -1)
		return 1;
	ctx = p->journal;
	while (fetch(ctx) != -1)
		if (exec(ctx) < 0)
			break;
	term(p);
	return 0;
}
