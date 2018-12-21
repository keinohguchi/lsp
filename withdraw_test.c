/* SPDX-License-Idetifier: GPL-2.0 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "withdraw.h"

struct thread_arg {
	pthread_t	tid;
	struct account	*account;
	unsigned int	amount;
	long int	ret;
};

static void *withdraw_thread(void *arg)
{
	struct thread_arg *t = (struct thread_arg *)arg;
	pthread_t me = pthread_self();
	if (!pthread_equal(me, t->tid)) {
		fprintf(stderr, "wrong thread ID:\n- want: 0x%ld\n-  got: 0x%ld\n",
			t->tid, me);
		return t;
	}
	t->ret = withdraw(t->account, t->amount);
	return t;
}

int main(void)
{
	const struct test {
		const char	*name;
		long int	initial_balance;
		unsigned int	withdraw_amount;
		int		withdraw_nr;
		long int	want;
	} tests[] = {
		{
			.name			= "900 withdraw from 1000 balance",
			.initial_balance	= 1000,
			.withdraw_amount	= 900,
			.withdraw_nr		= 1,
			.want			= 100,
		},
		{
			.name			= "9 100 withdraws from 1000 balance",
			.initial_balance	= 1000,
			.withdraw_amount	= 100,
			.withdraw_nr		= 9,
			.want			= 100,
		},
		{
			.name			= "100 10 withdraws from 1000 balance",
			.initial_balance	= 1000,
			.withdraw_amount	= 10,
			.withdraw_nr		= 100,
			.want			= 0,
		},
		{
			.name			= "1000 1 withdraws from 1000 balance",
			.initial_balance	= 1000,
			.withdraw_amount	= 1,
			.withdraw_nr		= 1000,
			.want			= 0,
		},
		{}, /* sentry */
	};
	const struct test *t;

	for (t = tests; t->name; t++) {
		struct thread_arg *args = NULL;
		struct account *a = NULL;
		int withdraw_nr;
		long int got;
		int ret, i;

		ret = 1;
		args = calloc(t->withdraw_nr, sizeof(struct thread_arg));
		if (!args) {
			perror("calloc");
			goto out;
		}
		a = open_account(t->initial_balance);
		if (!a) {
			fprintf(stderr, "%s: cannot open account\n", t->name);
			goto out;
		}
		withdraw_nr = t->withdraw_nr;
		for (i = 0; i < withdraw_nr; i++) {
			struct thread_arg *arg = &args[i];

			arg->account = a;
			arg->amount = t->withdraw_amount;
			ret = pthread_create(&arg->tid, NULL, withdraw_thread, arg);
			if (ret) {
				withdraw_nr = i;
				errno = ret;
				ret = 1;
				perror("pthread_create");
				goto test;
			}
		}
test:
		for (i = 0; i < withdraw_nr; i++) {
			struct thread_arg *arg = &args[i];
			ret = pthread_join(arg->tid, NULL);
			if (ret) {
				errno = ret;
				perror("pthread_join");
			}
		}
		ret = 0;
		got = balance(a);
		if (got != t->want) {
			fprintf(stderr, "%s: unexpected result:\n\t- want: %ld\n\t-  got: %ld\n",
				t->name, t->want, got);
			ret = 1;
		}
out:
		if (a)
			close_account(a);
		if (args)
			free(args);
		if (ret)
			return ret;
	}
	return 0;
}
