/*	$OpenBSD: mutex.c,v 1.13 2015/02/11 01:15:06 dlg Exp $	*/

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
#include <machine/lock.h>

#include <ddb/db_output.h>

static inline int
try_lock(struct mutex *mtx)
{
#ifdef MULTIPROCESSOR
	unsigned long t0, v0;

	__asm volatile(
		"1:	ldl_l	%0, %3		\n"	/* t0 = mtx->mtx_lock */
		"	bne	%0, 2f		\n"
		"	bis	$31, 1, %0	\n"	/* t0 = 1 */
		"	stl_c	%0, %2		\n"	/* mtx->mtx_lock = 1 */
		"	beq	%0, 3f		\n"
		"	mb			\n"
		"	bis	$31, 1, %1	\n"	/* v0 = 1 */
		"	br	4f		\n"
		"3:	br	1b		\n"	/* update failed */
		"2:	bis	$31, $31, %1	\n"	/* v0 = 0 */
		"4:				\n"
		: "=&r" (t0), "=r" (v0), "=m" (mtx->mtx_lock)
		: "m" (mtx->mtx_lock)
		: "memory");

	return (v0 != 0);
#else
	mtx->mtx_lock = 1;
	return 1;
#endif
}

void
__mtx_init(struct mutex *mtx, int wantipl)
{
	mtx->mtx_oldipl = IPL_NONE;
	mtx->mtx_wantipl = wantipl;
	mtx->mtx_lock = 0;
#ifdef MULTIPROCESSOR
	mtx->mtx_owner = NULL;
#endif
}

void
mtx_enter(struct mutex *mtx)
{
	int s;

	for (;;) {
		if (mtx->mtx_wantipl != IPL_NONE)
			s = _splraise(mtx->mtx_wantipl);
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

#ifdef MULTIPROCESSOR
		SPINLOCK_SPIN_HOOK;
#endif
	}
}

int
mtx_enter_try(struct mutex *mtx)
{
	int s;

	if (mtx->mtx_wantipl != IPL_NONE)
		s = _splraise(mtx->mtx_wantipl);
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
	int s;

	MUTEX_ASSERT_LOCKED(mtx);
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level--;
#endif
	s = mtx->mtx_oldipl;
	mtx->mtx_owner = NULL;
	mtx->mtx_lock = 0;
#ifdef MULTIPROCESSOR
	alpha_wmb();
#endif
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);
}
