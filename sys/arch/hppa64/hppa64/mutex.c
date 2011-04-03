/*	$OpenBSD: mutex.c,v 1.6 2011/04/03 18:46:40 miod Exp $	*/

/*
 * Copyright (c) 2004 Artur Grabowski <art@openbsd.org>
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <sys/param.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#ifdef MULTIPROCESSOR
#error This code needs more work
#endif

/*
 * Single processor systems don't need any mutexes, but they need the spl
 * raising semantics of the mutexes.
 */
void
mtx_init(struct mutex *mtx, int wantipl)
{
	mtx->mtx_oldipl = 0;
	mtx->mtx_wantipl = wantipl;
	mtx->mtx_lock = 0;
}

void
mtx_enter(struct mutex *mtx)
{
	mtx->mtx_oldipl = splraise(mtx->mtx_wantipl);
	MUTEX_ASSERT_UNLOCKED(mtx);
	mtx->mtx_lock = 1;
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level++;
#endif
}

int
mtx_enter_try(struct mutex *mtx)
{
	mtx->mtx_oldipl = splraise(mtx->mtx_wantipl);
	MUTEX_ASSERT_UNLOCKED(mtx);
	mtx->mtx_lock = 1;
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level++;
#endif

	return 1;
}

void
mtx_leave(struct mutex *mtx)
{
	MUTEX_ASSERT_LOCKED(mtx);
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level--;
#endif
	mtx->mtx_lock = 0;
	splx(mtx->mtx_oldipl);
}
