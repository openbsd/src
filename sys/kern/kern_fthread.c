/*	$OpenBSD: kern_fthread.c,v 1.1 1998/03/01 00:37:56 niklas Exp $	*/
/*	$NetBSD: kern_fthread.c,v 1.3 1998/02/07 16:23:35 chs Exp $	*/

/* 
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * The Mach Operating System project at Carnegie-Mellon University.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)kern_lock.c	8.1 (Berkeley) 6/11/93
 *
 *
 * Copyright (c) 1987, 1990 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Authors: Avadis Tevanian, Jr., Michael Wayne Young
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

/*
 *	Locking primitives implementation
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>

/*
 * these routines fake thread handling
 */

#if !defined(UVM)

void
assert_wait(event, ruptible)
	void *event;
	boolean_t ruptible;
{
#ifdef lint
	ruptible++;
#endif
	curproc->p_thread = event;
}

void
thread_block(msg)
char *msg;
{
	int s = splhigh();

	if (curproc->p_thread)
		tsleep(curproc->p_thread, PVM, msg, 0);
	splx(s);
}

#endif

void
thread_sleep_msg(event, lock, ruptible, msg, timo)
	void *event;
	simple_lock_t lock;
	boolean_t ruptible;
	char *msg;
	int timo;
{
	int s = splhigh();

#ifdef lint
	ruptible++;
#endif
	curproc->p_thread = event;
	simple_unlock(lock);
	if (curproc->p_thread)
		tsleep(event, PVM, msg, timo);
	splx(s);
}

/*
 * DEBUG stuff
 */

int indent = 0;

/*
 * Note that stdarg.h and the ANSI style va_start macro is used for both
 * ANSI and traditional C compilers.  (Same as subr_prf.c does.)
 * XXX: This requires that stdarg.h defines: va_alist, va_dcl
 */
#include <machine/stdarg.h>

/*ARGSUSED2*/
void
#ifdef	__STDC__
iprintf(int (*pr)(const char *, ...), const char *fmt, ...)
#else
iprintf(pr, fmt, va_alist)
	void (*pr)();
	const char *fmt;
	va_dcl
#endif
{
	register int i;
	va_list ap;

	va_start(ap, fmt);
	for (i = indent; i >= 8; i -= 8)
		(*pr)("\t");
	while (--i >= 0)
		(*pr)(" ");
#ifdef __powerpc__				/* XXX */
	if (pr != printf)			/* XXX */
		panic("iprintf");		/* XXX */
	vprintf(fmt, ap);			/* XXX */
#else						/* XXX */
	(*pr)("%:", fmt, ap);			/* XXX */
#endif /* __powerpc__ */			/* XXX */
	va_end(ap);
}
