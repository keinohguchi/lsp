/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LSP_WITHDRAW_H
#define _LSP_WITHDRAW_H

struct account {
	long int	balance;
	pthread_mutex_t	lock;
};

long int withdraw(struct account *account, unsigned int amount);
struct account *open_account(unsigned int deposit);
void close_account(struct account *account);
long int balance(struct account *account);

#endif /* _LSP_WITHDRAW_H */
