/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *rcsid = "$OpenBSD: abort.c,v 1.10 2002/11/03 23:58:39 marc Exp $";
#endif /* LIBC_SCCS and not lint */

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include "thread_private.h"
#include "atexit.h"

void
abort()
{
	struct atexit *p = __atexit;
	static int cleanup_called = 0;
	sigset_t mask;


	sigfillset(&mask);
	/*
	 * don't block SIGABRT to give any handler a chance; we ignore
	 * any errors -- X311J doesn't allow abort to return anyway.
	 */
	sigdelset(&mask, SIGABRT);
#ifdef _THREAD_SAFE
	(void)_thread_sys_sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#else  /* _THREAD_SAFE */
	(void)sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#endif /* _THREAD_SAFE */

	/*
	 * POSIX requires we flush stdio buffers on abort
	 */
	if (cleanup_called == 0) {
		while (p != NULL && p->next != NULL)
			p = p->next;
		if (p != NULL && p->fns[0] != NULL) {
			cleanup_called = 1;
			(*p->fns[0])();
		}
	}

	(void)kill(getpid(), SIGABRT);

	/*
	 * if SIGABRT ignored, or caught and the handler returns, do
	 * it again, only harder.
	 */
	(void)signal(SIGABRT, SIG_DFL);
#ifdef _THREAD_SAFE
	(void)_thread_sys_sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#else  /* _THREAD_SAFE */
	(void)sigprocmask(SIG_SETMASK, &mask, (sigset_t *)NULL);
#endif /* _THREAD_SAFE */
	(void)kill(getpid(), SIGABRT);
	exit(1);
}
