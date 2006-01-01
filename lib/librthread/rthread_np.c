/*	$OpenBSD: rthread_np.c,v 1.3 2006/01/01 18:53:25 otto Exp $	*/
/*
 * Copyright (c) 2004,2005 Ted Unangst <tedu@openbsd.org>
 * Copyright (c) 2005 Otto Moerbeek <otto@openbsd.org>
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

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/queue.h>

#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <string.h>
#include <unistd.h>

#include <uvm/uvm_extern.h>
#include <machine/spinlock.h>

#include "rthread.h"

void
pthread_set_name_np(pthread_t thread, char *name)
{
	strlcpy(thread->name, name, sizeof(thread->name));
}

int
pthread_main_np(void)
{
	return (!_threads_ready || getthrid() == _initial_thread.tid ? 1 : 0);
}


/*
 * Return stack info from the given thread.  Based upon the solaris
 * thr_stksegment function.
 *
 * This function taken from the uthread library, with the following
 * license: 
 * PUBLIC DOMAIN: No Rights Reserved. Marco S Hyman <marc@snafu.org> */
int
pthread_stackseg_np(pthread_t thread, stack_t *sinfo)
{
	char *base;
	size_t pgsz;
	int ret;

	if (thread->stack) {
		base = thread->stack->base;
#if !defined(MACHINE_STACK_GROWS_UP)
		base += thread->stack->len;
#endif
		sinfo->ss_sp = base;
		sinfo->ss_size = thread->stack->len;
		sinfo->ss_flags = 0;
		ret = 0;
	} else if (thread == &_initial_thread) {
		pgsz = sysconf(_SC_PAGESIZE);
		if (pgsz == (size_t)-1)
			ret = EAGAIN;
		else {
#if defined(MACHINE_STACK_GROWS_UP)
			base = (caddr_t) USRSTACK;
#else
			base = (caddr_t) ((USRSTACK - DFLSSIZ) & ~(pgsz - 1));
			base += DFLSSIZ;
#endif
			sinfo->ss_sp = base;
			sinfo->ss_size = DFLSSIZ;
			sinfo->ss_flags = 0;
			ret = 0;
		}

	} else
		ret = EAGAIN;

	return ret;
}
