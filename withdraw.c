/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <limits.h>
#include <pthread.h>
#include <errno.h>

const char *progname;

struct account {
	long int	balance;
	pthread_mutex_t	lock;
};

struct withdrawer {
	pthread_t	tid;
	struct account	*account;
	unsigned int	amount;
	long int	balance;
};

static long int withdraw(struct account *account, unsigned int amount)
{
	long int balance;

	pthread_mutex_lock(&account->lock);
	if (account->balance < amount) {
		pthread_mutex_unlock(&account->lock);
		return -1;
	}
	account->balance -= amount;
	balance = account->balance;
	pthread_mutex_unlock(&account->lock);
	return balance;
}

static struct account *open_account(unsigned int deposit)
{
	struct account *account;
	int ret;

	account = malloc(sizeof(struct account));
	if (!account)
		return NULL;
	ret = pthread_mutex_init(&account->lock, NULL);
	if (ret) {
		errno = ret;
		perror("pthread_mutex_init");
		free(account);
		return NULL;
	}
	account->balance = deposit;
	return account;
}

static void close_account(struct account *account)
{
	if (!account)
		return;
	pthread_mutex_destroy(&account->lock);
	free(account);
}

static long int balance(struct account *account)
{
	long int balance;
	pthread_mutex_lock(&account->lock);
	balance = account->balance;
	pthread_mutex_unlock(&account->lock);
	return balance;
}

/* withdrawer thread */
static void *withdrawer(void *arg)
{
	struct withdrawer *w = (struct withdrawer *)arg;
	pthread_t me = pthread_self();

	if (!pthread_equal(me, w->tid)) {
		fprintf(stderr, "wrong thread\n");
		return w;
	}
	w->balance = withdraw(w->account, w->amount);
	if (w->balance == -1)
		fprintf(stderr, "cannot withdraw\n");
	return arg;
}

static void usage(FILE *stream, int status, const char *const opts,
		  const struct option *const lopts)
{
	const struct option *o;
	fprintf(stream, "usage: %s [-%s]\n", progname, opts);
	fprintf(stream, "options:\n");
	for (o = lopts; o->name; o++) {
		fprintf(stream, "\t-%c,--%s", o->val, o->name);
		switch (o->val) {
		case 'c':
			fprintf(stream, " X\tnumber of withdraw (default:1)\n");
			break;
		case 'd':
			fprintf(stream, " X\tamount of initial deposit (default:1000)\n");
			break;
		case 'w':
			fprintf(stream, " X\tamount of each withdrawal (default:100)\n");
			break;
		case 'h':
			fprintf(stream, "\tshow this message\n");
			break;
		default:
			fprintf(stream, "\n");
		}
	}
	exit(status);
}

int main(int argc, char *argv[])
{
	const char *opts = "hc:d:w:";
	const struct option lopts[] = {
		{"count",	required_argument,	NULL, 'c'},
		{"deposit",	required_argument,	NULL, 'd'},
		{"withdraw",	required_argument,	NULL, 'w'},
		{"help",	no_argument,		NULL, 'h'},
		{}, /* sentry */
	};
	struct withdrawer *warg, *wargs, *wret;
	struct account *account;
	unsigned int deposit, withdraw;
	long int nr, i;
	int ret, opt;

	/* command line option handling */
	nr = 1;
	deposit = 1000;
	withdraw = 100;
	progname = argv[0];
	while ((opt = getopt_long(argc, argv, opts, lopts, NULL)) != -1) {
		switch (opt) {
		case 'c':
			nr = strtol(optarg, NULL, 10);
			if (nr <= 0 || nr >= LONG_MAX)
				usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		case 'd':
			deposit = strtol(optarg, NULL, 10);
			if (deposit < 0 || deposit >= LONG_MAX)
				usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		case 'w':
			withdraw = strtol(optarg, NULL, 10);
			if (withdraw < 0 || withdraw >= LONG_MAX)
				usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		case 'h':
			usage(stdout, EXIT_SUCCESS, opts, lopts);
			break;
		case '?':
		default:
			usage(stderr, EXIT_FAILURE, opts, lopts);
			break;
		}
	}

	ret = 1;
	account = NULL;
	wargs = calloc(nr, sizeof(struct withdrawer));
	if (!wargs) {
		perror("calloc");
		goto out;
	}
	account = open_account(deposit);
	if (!account) {
		fprintf(stderr, "cannot open account\n");
		goto out;
	}
	warg = NULL;
	for (i = 0; i < nr; i++) {
		warg = &wargs[i];
		warg->account = account;
		warg->amount = withdraw;
		ret = pthread_create(&warg->tid, NULL, withdrawer, warg);
		if (ret) {
			errno = ret;
			perror("pthread_create");
			nr = i;
			ret = 1;
			goto join;
		}
	}
join:
	for (i = 0; i < nr; i++) {
		warg = &wargs[i];
		ret = pthread_join(warg->tid, (void **)&wret);
		if (ret) {
			errno = ret;
			perror("pthread_join");
			ret = 1;
			continue;
		}
	}
	printf("balance=%ld\n", balance(account));
out:
	if (account)
		close_account(account);
	if (wargs)
		free(wargs);
	return ret;
}
