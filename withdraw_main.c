/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "withdraw.h"

int main(int argc, char *argv[])
{
	unsigned int deposit = 1000;
	unsigned int amount = 100;
	struct account *account;
	long int current_balance;
	int ret;

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
	account = open_account(deposit);
	if (!account) {
		fprintf(stderr, "cannot open account\n");
		return 1;
	}
	ret = 0;
	current_balance = withdraw(account, amount);
	if (current_balance == -1) {
		ret = 1;
		fprintf(stderr, "cannot withdraw amount(%u), current balance(%ld)\n",
			amount, balance(account));
		goto out;
	}
	printf("withdraw=%u,balance=%ld\n", amount, current_balance);
out:
	close_account(account);
	return ret;
}
