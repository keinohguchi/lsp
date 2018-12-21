/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "withdraw.h"

struct withdraw_arg {
	pthread_t	tid;
	struct account	*account;
	unsigned int	amount;
	long int	balance;
};

static void *withdraw_thread(void *arg)
{
	struct withdraw_arg *a = (struct withdraw_arg *)arg;
	pthread_t me = pthread_self();
	if (!pthread_equal(me, a->tid)) {
		fprintf(stderr, "wrong thread\n");
		return a;
	}
	a->balance = withdraw(a->account, a->amount);
	if (a->balance == -1)
		fprintf(stderr, "cannot withdraw\n");
	return a;
}

int main(int argc, char *argv[])
{
	struct withdraw_arg *args = NULL;
	struct withdraw_arg *arg;
	struct account *account = NULL;
	unsigned int deposit = 1000;
	unsigned int amount = 100;
	long int nr = 1;
	int ret, i;

	if (argc > 1) {
		long int val;
		
		errno = 0;
		val = strtol(argv[1], NULL, 10);
		if (errno) {
			perror("strtol");
			return 1;
		}
		amount = val;
	}
	if (argc > 2) {
		long int val;

		errno = 0;
		val = strtol(argv[2], NULL, 10);
		if (errno) {
			perror("strtol");
			return 1;
		}
		deposit = val;
	}
	if (argc > 3) {
		long int val;

		errno = 0;
		val = strtol(argv[3], NULL, 10);
		if (errno) {
			perror("strtol");
			return 1;
		}
		nr = val;
	}
	ret = 1;
	args = calloc(nr, sizeof(struct withdraw_arg));
	if (!args) {
		perror("calloc");
		goto out;
	}
	account = open_account(deposit);
	if (!account) {
		fprintf(stderr, "cannot open account\n");
		goto out;
	}
	for (i = 0; i < nr; i++) {
		arg = &args[i];
		arg->account = account;
		arg->amount = amount;
		ret = pthread_create(&arg->tid, NULL, withdraw_thread, arg);
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
		arg = &args[i];
		ret = pthread_join(arg->tid, NULL);
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
	if (args)
		free(args);
	return ret;
}
