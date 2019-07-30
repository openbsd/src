/*	$OpenBSD: futex.c,v 1.2 2017/04/30 10:11:03 mpi Exp $ */
/*
 * Copyright (c) 2017 Martin Pieuchot
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/time.h>
#include <sys/futex.h>

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#include "futex.h"

uint32_t lock = 0;

void handler(int);
void *signaled(void *);
void *awakener(void *);

int
main(int argc, char *argv[])
{
	struct sigaction sa;
	struct timespec abs = { 0, 5000 };
	pthread_t thread;

	/* Invalid operation */
	assert(futex(&lock, 0xFFFF, 0, 0, NULL) == ENOSYS);

	/* Incorrect pointer */
	assert(futex_twait((void *)0xdeadbeef, 1, 0, NULL) == EFAULT);

	/* If (lock != 1) return EAGAIN */
	assert(futex_twait(&lock, 1, 0, NULL) == EAGAIN);

	/* Deadlock for 5000ns */
	assert(futex_twait(&lock, 0, CLOCK_REALTIME, &abs) == ETIMEDOUT);

	/* Interrupt a thread waiting on a futex. */
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = handler;
	assert(sigaction(SIGUSR1, &sa, NULL) == 0);
	assert(pthread_create(&thread, NULL, signaled, NULL) == 0);
	usleep(100);
	assert(pthread_kill(thread, SIGUSR1) == 0);
	assert(pthread_join(thread, NULL) == 0);

	/* Wait until another thread awakes us. */
	assert(pthread_create(&thread, NULL, awakener, NULL) == 0);
	assert(futex_twait(&lock, 0, 0, NULL) == 0);
	assert(pthread_join(thread, NULL) == 0);

	return 0;
}

void
handler(int sig)
{
}

void *
signaled(void *arg)
{
	/* Wait until receiving a signal. */
	assert(futex_twait(&lock, 0, 0, NULL) == EINTR);
}

void *
awakener(void *arg)
{
	usleep(100);
	assert(futex_wake(&lock, -1) == 1);
}
