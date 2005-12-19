/*	$OpenBSD: rthread_attr.c,v 1.2 2005/12/19 06:45:14 tedu Exp $ */
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
 * generic attribute support
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

int
pthread_attr_init(pthread_attr_t *attrp)
{
	pthread_attr_t attr;

	attr = malloc(sizeof(*attr));
	if (!attr)
		return (errno);
	memset(attr, 0, sizeof(*attr));
	*attrp = attr;

	return (0);
}

int
pthread_attr_destroy(pthread_attr_t *attrp)
{
	free(*attrp);
	*attrp = NULL;

	return (0);
}

int
pthread_attr_getdetachstate(const pthread_attr_t *attrp, int *detachstate)
{
	*detachstate = (*attrp)->detach_state;

	return (0);
}

int
pthread_attr_setdetachstate(pthread_attr_t *attrp, int detachstate)
{
	(*attrp)->detach_state = detachstate;

	return (0);
}

int
pthread_attr_getstack(const pthread_attr_t *attrp, void **stackaddr,
    size_t *stacksize)
{
	*stackaddr = (*attrp)->stack_addr;
	*stacksize = (*attrp)->stack_size;

	return (0);
}

int
pthread_attr_setstack(pthread_attr_t *attrp, void *stackaddr, size_t stacksize)
{
	(*attrp)->stack_addr = stackaddr;
	(*attrp)->stack_size = stacksize;

	return (0);
}

int
pthread_attr_getstacksize(const pthread_attr_t *attrp, size_t *stacksize)
{
	*stacksize = (*attrp)->stack_size;

	return (0);
}

int
pthread_attr_setstacksize(pthread_attr_t *attrp, size_t stacksize)
{
	(*attrp)->stack_size = stacksize;

	return (0);
}

int
pthread_attr_getstackaddr(const pthread_attr_t *attrp, void **stackaddr)
{
	*stackaddr = (*attrp)->stack_addr;

	return (0);
}

int
pthread_attr_setstackaddr(pthread_attr_t *attrp, void *stackaddr)
{
	(*attrp)->stack_addr = stackaddr;

	return (0);
}

int
pthread_attr_getscope(const pthread_attr_t *attrp, int *contentionscope)
{
	*contentionscope = (*attrp)->contention_scope;

	return (0);
}

int
pthread_attr_setscope(pthread_attr_t *attrp, int contentionscope)
{
	(*attrp)->contention_scope = contentionscope;

	return (0);
}
