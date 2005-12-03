/*	$OpenBSD: mutex.S,v 1.1 2005/12/03 19:01:14 miod Exp $	*/

/*
 * Copyright (c) 2005, Miodrag Vallat.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"
#if MTX_LOCK != 0
#error	Lack of Humppa in mutex code
#endif

#include <machine/asm.h>

/*
 * void mtx_init(struct mutex *mtx, int ipl)
 */
ENTRY(mtx_init)
	st	r0, r2, MTX_LOCK	/* mtx->mtx_lock = 0 */
	st	r0, r2, MTX_OLDIPL	/* mtx->mtx_oldipl = IPL_NONE */
	st	r3, r2, MTX_WANTIPL	/* mtx->mtx_wantipl = ipl */
	jmp.n	r1
	 st	r0, r2, MTX_CPU		/* mtx->mtx_cpu = NULL */

/*
 * void mtx_enter(struct mutex *mtx)
 */
ENTRY(mtx_enter)
	subu	r31, r31, 8
	st	r1,  r31, 4		/* save return address */

#ifdef MULTIPROCESSOR

	st	r2,  r31, 0		/* save mtx */
enter_again:
	bsr.n	_C_LABEL(raiseipl)	/* raiseipl(mtx->mtx_wantipl) */
	 ld	r2,  r2,  MTX_WANTIPL

	ld	r4,  r31, 0
	or	r3,  r0,  1
	xmem	r3,  r4,  r0		/* attempt to claim the lock, old */
	bcnd	ne0, r3,  enter_failed	/* mtx->mtx_lock is 0 if successful */

	st	r2,  r4,  MTX_OLDIPL	/* save into mtx_oldipl */
	ldcr	r3,  CPU
	st	r3,  r4,  MTX_CPU	/* mtx->mtx_cpu = curcpu() */

	ld	r1,  r31, 4
	jmp.n	r1
	 addu	r31, r31, 8

enter_failed:	/* the lock is not ours... */
	bcnd	ne0, r2,  1f		/* splx(oldipl) */
	bsr.n	_C_LABEL(spl0)
	 addu	r1,  r1,  2f - . - 4
1:
	bsr	_C_LABEL(setipl)
2:
	ld	r2,  r31, 0		/* restore mtx */
enter_spin:
#ifdef DIAGNOSTIC
	ldcr	r3,  CPU
	ld	r4,  r2,  MTX_CPU
	cmp	r5,  r3,  r4
	bcnd	eq0, r5,  enter_panic
#endif
	ld	r3,  r2,  MTX_LOCK
	bcnd	eq0, r3,  enter_again
	br	enter_spin

#ifdef DIAGNOSTIC
enter_panic:
	or.u	r2,  r0,  hi16(9f)
	bsr.n	_C_LABEL(panic)
	 or	r2,  r2,  lo16(9f)

	data
9:
	string	"mtx_enter: humpaan itsekseni"
#endif

#else	/* MULTIPROCESSOR */

	st	r2,  r31, 0		/* save mtx */
	bsr.n	_C_LABEL(raiseipl)	/* raiseipl(mtx->mtx_wantipl) */
	 ld	r2,  r2,  MTX_WANTIPL

	ld	r4,  r31, 0
	or	r3,  r0,  1
	st	r3,  r4,  MTX_LOCK	/* locked! */

	st	r2,  r4,  MTX_OLDIPL	/* save into mtx_oldipl */

#ifdef DIAGNOSTIC	/* necessary for MUTEX_ASSERT_LOCKED */
	ldcr	r3,  CPU
	st	r3,  r4,  MTX_CPU	/* mtx->mtx_cpu = curcpu() */
#endif

	ld	r1,  r31, 4
	jmp.n	r1
	 addu	r31, r31, 8

#endif	/* MULTIPROCESSOR */
	
/*
 * void mtx_leave(struct mutex *mtx)
 */
ENTRY(mtx_leave)
	subu	r31, r31, 8
	st	r1,  r31, 0

	ld	r3,  r2,  MTX_OLDIPL
	st	r0,  r2,  MTX_CPU		/* mtx->mtx_cpu = NULL */
#ifdef DEBUG
	st	r0,  r2,  MTX_OLDIPL		/* mtx->mtx_oldipl = IPL_NONE */
#endif

	bcnd.n	ne0, r3,  1f			/* splx(mtx->mtx_oldipl) */
	 st	r0,  r2,  MTX_LOCK		/* mtx->mtx_lock = 0 */
	bsr.n	_C_LABEL(spl0)
	 addu	r1,  r1,  2f - . - 4
1:
	bsr.n	_C_LABEL(setipl)
	 or	r2,  r3,  r0
2:
	ld	r1,  r31, 0
	jmp.n	r1
	 addu	r31, r31, 8
