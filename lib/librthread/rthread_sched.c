/*	$OpenBSD: rthread_sched.c,v 1.1 2005/12/03 18:16:19 tedu Exp $ */
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
 * scheduling routines
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

int yield(void);

int
pthread_getschedparam(pthread_t thread, int *policy,
    struct sched_param *param)
{
	*policy = thread->sched_policy;
	if (param)
		*param = thread->sched_param;

	return (0);
}

int
pthread_setschedparam(pthread_t thread, int policy,
    const struct sched_param *param)
{
	thread->sched_policy = policy;
	if (param)
		thread->sched_param = *param;

	return (0);
}

int
pthread_attr_getschedparam(const pthread_attr_t *attrp, struct sched_param *param)
{
	*param = (*attrp)->sched_param;

	return (0);
}

int
pthread_attr_setschedparam(pthread_attr_t *attrp, const struct sched_param *param)
{
	(*attrp)->sched_param = *param;

	return (0);
}

int
pthread_attr_getschedpolicy(const pthread_attr_t *attrp, int *policy)
{
	*policy = (*attrp)->sched_policy;

	return (0);
}

int
pthread_attr_setschedpolicy(pthread_attr_t *attrp, int policy)
{
	(*attrp)->sched_policy = policy;

	return (0);
}

int
pthread_attr_getinheritsched(const pthread_attr_t *attrp, int *inherit)
{
	*inherit = (*attrp)->sched_inherit;

	return (0);
}

int
pthread_attr_setinheritsched(pthread_attr_t *attrp, int inherit)
{
	(*attrp)->sched_inherit = inherit;

	return (0);
}

int
pthread_getprio(pthread_t thread)
{
	return (thread->sched_param.sched_priority);
}

int
pthread_setprio(pthread_t thread, int priority)
{
	thread->sched_param.sched_priority = priority;

	return (0);
}

void
pthread_yield(void)
{
	yield();
}
int
sched_yield(void)
{
	yield();

	return (0);
}
