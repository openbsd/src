/*
 * Copyright (c) 2002 Daniel Hartmeier
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: atexit.c,v 1.7 2002/09/14 22:03:14 dhartmei Exp $";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/mman.h>
#include <stdlib.h>
#include <unistd.h>
#include "atexit.h"

int __atexit_invalid = 1;
struct atexit *__atexit;

/*
 * Function pointers are stored in a linked list of pages. The list
 * is initially empty, and pages are allocated on demand. The first
 * function pointer in the first allocated page (the last one in
 * the linked list) is reserved for the cleanup function.
 *
 * Outside the following two functions, all pages are mprotect()'ed
 * to prevent unintentional/malicious corruption.
 *
 * The free(malloc(1)) is a workaround causing malloc_init() to
 * ensure that malloc.c gets the first mmap() call for its sbrk()
 * games.
 */

/*
 * Register a function to be performed at exit.
 */
int
atexit(fn)
	void (*fn)();
{
	register struct atexit *p = __atexit;
	register int pgsize = getpagesize();

	if (pgsize < sizeof(*p))
		return (-1);
	if (p != NULL) {
		if (p->ind + 1 >= p->max)
			p = NULL;
		else if (mprotect(p, pgsize, PROT_READ | PROT_WRITE))
			return (-1);
	}
	if (p == NULL) {
		if (__atexit_invalid) {
			free(malloc(1));
			__atexit_invalid = 0;
		}
		p = mmap(NULL, pgsize, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE, -1, 0);
		if (p == MAP_FAILED)
			return (-1);
		if (__atexit == NULL) {
			p->fns[0] = NULL;
			p->ind = 1;
		} else
			p->ind = 0;
		p->max = (pgsize - ((char *)&p->fns[0] - (char *)p)) /
		    sizeof(p->fns[0]);
		p->next = __atexit;
		__atexit = p;
	}
	p->fns[p->ind++] = fn;
	if (mprotect(p, pgsize, PROT_READ))
		return (-1);
	return (0);
}

/*
 * Register the cleanup function
 */
void
__atexit_register_cleanup(fn)
	void (*fn)();
{
	register struct atexit *p = __atexit;
	register int pgsize = getpagesize();

	if (pgsize < sizeof(*p))
		return;
	while (p != NULL && p->next != NULL)
		p = p->next;
	if (p == NULL) {
		if (__atexit_invalid) {
			free(malloc(1));
			__atexit_invalid = 0;
		}
		p = mmap(NULL, pgsize, PROT_READ | PROT_WRITE,
		    MAP_ANON | MAP_PRIVATE, -1, 0);
		if (p == MAP_FAILED)
			return;
		p->ind = 1;
		p->max = (pgsize - ((char *)&p->fns[0] - (char *)p)) /
		    sizeof(p->fns[0]);
		p->next = NULL;
		__atexit = p;
	} else {
		if (mprotect(p, pgsize, PROT_READ | PROT_WRITE))
		    return;
	}
	p->fns[0] = fn;
	mprotect(p, pgsize, PROT_READ);
}
