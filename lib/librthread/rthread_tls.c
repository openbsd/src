/*	$OpenBSD: rthread_tls.c,v 1.6 2005/12/18 01:35:06 tedu Exp $ */
/*
 * Copyright (c) 2004 Ted Unangst <tedu@openbsd.org>
 * All Rights Reserved.
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
/*
 * thread specific storage
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <machine/spinlock.h>

#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <pthread.h>

#include "rthread.h"

static struct rthread_key rkeys[PTHREAD_KEYS_MAX];
static _spinlock_lock_t rkeyslock = _SPINLOCK_UNLOCKED;

int
pthread_key_create(pthread_key_t *key, void (*destructor)(void*))
{
	static int hint;
	int i;

	_spinlock(&rkeyslock);
	if (rkeys[hint].used) {
		for (i = 0; i < PTHREAD_KEYS_MAX; i++) {
			if (!rkeys[i].used)
				break;
		}
		if (i == PTHREAD_KEYS_MAX) {
			_spinunlock(&rkeyslock);
			return (EAGAIN);
		}
		hint = i;
	}
	rkeys[hint].used = 1;
	rkeys[hint].destructor = destructor;

	*key = hint++;
	if (hint >= PTHREAD_KEYS_MAX)
		hint = 0;
	_spinunlock(&rkeyslock);

	return (0);
}

int
pthread_key_delete(pthread_key_t key)
{

	if (rkeys[key].used)
		return (EINVAL);

	_spinlock(&rkeyslock);
	rkeys[key].used = 0;
	rkeys[key].destructor = NULL;
	_spinunlock(&rkeyslock);

	return (0);
}

static struct rthread_storage *
rthread_findstorage(pthread_key_t key)
{
	struct rthread_storage *rs;
	pthread_t self;

	self = pthread_self();

	for (rs = self->local_storage; rs; rs = rs->next) {
		if (rs->keyid == key)
			break;
	}
	if (!rs) {
		rs = malloc(sizeof(*rs));
		if (!rs)
			return (NULL);
		memset(rs, 0, sizeof(*rs));
		rs->keyid = key;
		rs->data = NULL;
		rs->next = self->local_storage;
		self->local_storage = rs;
	}

	return (rs);
}

void *
pthread_getspecific(pthread_key_t key)
{
	struct rthread_storage *rs;

	rs = rthread_findstorage(key);
	if (!rs)
		return (NULL);

	return (rs->data);
}

int
pthread_setspecific(pthread_key_t key, const void *data)
{
	struct rthread_storage *rs;

	rs = rthread_findstorage(key);
	if (!rs)
		return (ENOMEM);
	rs->data = (void *)data;

	return (0);
}

void
rthread_tls_destructors(pthread_t thread)
{
	struct rthread_storage *rs;
	int i;

	_spinlock(&rkeyslock);
	for (i = 0; i < PTHREAD_DESTRUCTOR_ITERATIONS; i++) {
		for (rs = thread->local_storage; rs; rs = rs->next) {
			if (!rs->data)
				continue;
			if (rkeys[rs->keyid].destructor) {
				void (*destructor)(void *) =
				    rkeys[rs->keyid].destructor;
				void *data = rs->data;
				_spinunlock(&rkeyslock);
				rs->data = NULL;
				destructor(data);
				_spinlock(&rkeyslock);
			}
		}
	}
	_spinunlock(&rkeyslock);
}
