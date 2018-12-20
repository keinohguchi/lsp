/* SPDX-License-Idetifier: GPL-2.0 */
#include <stdio.h>
#include <pthread.h>

#include "withdraw.h"

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
		{}, /* sentry */
	};
	const struct test *t;

	for (t = tests; t->name; t++) {
		struct account *a;
		long int got;
		int ret, i;
		
		a = open_account(t->initial_balance);
		if (!a) {
			fprintf(stderr, "%s: cannot open account\n", t->name);
			return 1;
		}
		ret = 0;
		for (i = 0; i < t->withdraw_nr; i++) {
			long int balance = withdraw(a, t->withdraw_amount);
			if (balance == -1) {
				ret = 1;
				fprintf(stderr, "%s: cannot withdraw ammount: %u\n",
					t->name, t->withdraw_amount);
			}
		}
		got = balance(a);
		if (got != t->want) {
			ret = 1;
			fprintf(stderr, "%s: unexpected result:\n- want: %ld\n-  got: %ld\n",
				t->name, t->want, got);
		}
		close_account(a);
		if (ret)
			return ret;
	}
	return 0;
}
