/*	$OpenBSD: mutex.c,v 1.1 2015/08/14 06:14:19 dlg Exp $	*/

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
#include <sys/atomic.h>

#include <machine/intr.h>
#include <machine/lock.h>

#include <ddb/db_output.h>

void
__mtx_init(struct mutex *mtx, int ipl)
{
	mtx->mtx_owner = NULL;
	mtx->mtx_wantipl = ipl;
	mtx->mtx_oldipl = IPL_NONE;
}

#ifdef MULTIPROCESSOR
#if defined(MP_LOCKDEBUG)
#ifndef DDB
#error "MP_LOCKDEBUG requires DDB"
#endif

/* CPU-dependent timing, needs this to be settable from ddb. */
extern int __mp_lock_spinout;
#endif

void
mtx_enter(struct mutex *mtx)
{
#if defined(MP_LOCKDEBUG)
	int ticks = __mp_lock_spinout;
#endif

	while (mtx_enter_try(mtx) == 0) {
		SPINLOCK_SPIN_HOOK;

#if defined(MP_LOCKDEBUG)
		if (--ticks == 0) {
			db_printf("%s: %p lock spun out", __func__, mtx);
			Debugger();
		}
#endif
	}
}

int
mtx_enter_try(struct mutex *mtx)
{
	struct cpu_info *owner, *ci = curcpu();
	int s;

	if (mtx->mtx_wantipl != IPL_NONE)
		s = splraise(mtx->mtx_wantipl);

	owner = atomic_cas_ptr(&mtx->mtx_owner, NULL, ci);
#ifdef DIAGNOSTIC
	if (__predict_false(owner == ci))
		panic("mtx %p: locking against myself", mtx);
#endif
	if (owner == NULL) {
		if (mtx->mtx_wantipl != IPL_NONE)
			mtx->mtx_oldipl = s;
#ifdef DIAGNOSTIC
		ci->ci_mutex_level++;
#endif
		membar_enter();
		return (1);
	}

	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);

	return (0);
}
#else
void
mtx_enter(struct mutex *mtx)
{
	struct cpu_info *ci = curcpu();

#ifdef DIAGNOSTIC
	if (__predict_false(mtx->mtx_owner == ci))
		panic("mtx %p: locking against myself", mtx);
#endif
	if (mtx->mtx_wantipl != IPL_NONE)
		mtx->mtx_oldipl = splraise(mtx->mtx_wantipl);

	mtx->mtx_owner = ci;

#ifdef DIAGNOSTIC
	ci->ci_mutex_level++;
#endif
}

int
mtx_enter_try(struct mutex *mtx)
{
	mtx_enter(mtx);
	return (1);
}
#endif

void
mtx_leave(struct mutex *mtx)
{
	int s;

	MUTEX_ASSERT_LOCKED(mtx);

#ifdef MULTIPROCESSOR
	membar_exit();
#endif
#ifdef DIAGNOSTIC
	curcpu()->ci_mutex_level--;
#endif

	s = mtx->mtx_oldipl;
	mtx->mtx_owner = NULL;
	if (mtx->mtx_wantipl != IPL_NONE)
		splx(s);
}
