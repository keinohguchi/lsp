/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <pthread.h>

#include "withdraw.h"

long int withdraw(struct account *account, unsigned int amount)
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

struct account *open_account(unsigned int deposit)
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

void close_account(struct account *account)
{
	if (!account)
		return;
	pthread_mutex_destroy(&account->lock);
	free(account);
}

long int balance(struct account *account)
{
	long int balance;
	pthread_mutex_lock(&account->lock);
	balance = account->balance;
	pthread_mutex_unlock(&account->lock);
	return balance;
}
