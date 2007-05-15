/*	$OpenBSD: pxa2x0_mutex.c,v 1.1 2007/05/15 05:26:44 miod Exp $	*/

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

#include <machine/intr.h>

#ifdef MULTIPROCESSOR
#error This code needs work
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
	if (mtx->mtx_wantipl != IPL_NONE)
		mtx->mtx_oldipl = _splraise(mtx->mtx_wantipl);

	MUTEX_ASSERT_UNLOCKED(mtx);
	mtx->mtx_lock = 1;
}

void
mtx_leave(struct mutex *mtx)
{
	MUTEX_ASSERT_LOCKED(mtx);
	mtx->mtx_lock = 0;
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(mtx->mtx_oldipl);
}
