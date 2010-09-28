/*	$OpenBSD: mutex.c,v 1.9 2010/09/28 20:27:55 miod Exp $	*/

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

static inline int
try_lock(struct mutex *mtx)
{
#ifdef MULTIPROCESSOR
	int tmp, ret = 0;

        asm volatile (
		".set noreorder\n"
		"ll	%0, %2\n"
		"bnez	%0, 1f\n"
		"nop\n"
		"li	%1, 1\n"
		"sc	%1, %2\n"
		"1:\n"
		".set reorder\n"
		: "+r"(tmp), "+r"(ret)
		: "m"(mtx->mtx_lock));
	
	return ret;
#else  /* MULTIPROCESSOR */
	mtx->mtx_lock = 1;
	return 1;
#endif /* MULTIPROCESSOR */
}

void
mtx_init(struct mutex *mtx, int wantipl)
{
	mtx->mtx_lock = 0;
	mtx->mtx_wantipl = wantipl;
	mtx->mtx_oldipl = IPL_NONE;
}

void
mtx_enter(struct mutex *mtx)
{
	int s;

	for (;;) {
		if (mtx->mtx_wantipl != IPL_NONE)
			s = splraise(mtx->mtx_wantipl);
		if (try_lock(mtx)) {
			if (mtx->mtx_wantipl != IPL_NONE)
				mtx->mtx_oldipl = s;
			mtx->mtx_owner = curcpu();
#ifdef DIAGNOSTIC
			curcpu()->ci_mutex_level++;
#endif
			return;
		}
		if (mtx->mtx_wantipl != IPL_NONE)
			splx(s);
	}
}

int
mtx_enter_try(struct mutex *mtx)
{
	int s;
	
 	if (mtx->mtx_wantipl != IPL_NONE)
		s = splraise(mtx->mtx_wantipl);
	if (try_lock(mtx)) {
		if (mtx->mtx_wantipl != IPL_NONE)
			mtx->mtx_oldipl = s;
		mtx->mtx_owner = curcpu();
#ifdef DIAGNOSTIC
		curcpu()->ci_mutex_level++;
#endif
		return 1;
	}
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);
	return 0;
}

void
mtx_leave(struct mutex *mtx)
{
	MUTEX_ASSERT_LOCKED(mtx);
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level--;
#endif
	mtx->mtx_lock = 0;
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(mtx->mtx_oldipl);
	mtx->mtx_owner = NULL;
}
