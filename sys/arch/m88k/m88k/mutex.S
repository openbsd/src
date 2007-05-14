/*	$OpenBSD: mutex.S,v 1.3 2007/05/14 16:58:42 miod Exp $	*/

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
	st	r2,  r31, 0		/* save mtx */

#ifdef MULTIPROCESSOR
enter_again:
	or	r3,  r0,  1
	xmem	r3,  r2,  r0		/* attempt to claim the lock, old */
	bcnd	eq0, r3,  enter_success	/* mtx->mtx_lock is 0 if successful */
#ifdef DIAGNOSTIC
	ldcr	r3,  CPU
	ld	r4,  r2,  MTX_CPU
	cmp	r5,  r3,  r4
	bcnd	eq0, r5,  enter_panic	/* ... unless it is, oops */
#endif
enter_spin:
	/* Avoid clogging the bus with xmem, wait until the mutex is released */
	ld	r3,  r2,  MTX_LOCK
	bcnd	eq0, r3,  enter_again
	br	enter_spin
#ifdef DIAGNOSTIC
enter_panic:
	or.u	r2,  r0,  hi16(9f)
	bsr.n	_C_LABEL(panic)
	 or	r2,  r2,  lo16(9f)

	data
9:	string	"mtx_enter: humpaan itsekseni"
	text
#endif
enter_success:
#else	/* MULTIPROCESSOR */
	st	r2,  r2,  MTX_LOCK	/* locked! */
#endif	/* MULTIPROCESSOR */

	ldcr	r3,  CPU
	st	r3,  r2,  MTX_CPU	/* mtx->mtx_cpu = curcpu() */
	ld	r2,  r2,  MTX_WANTIPL
	bcnd	eq0, r2,  1f		/* if mtx_wantipl == IPL_NONE, skip */
	bsr	_C_LABEL(raiseipl)	/* raiseipl(mtx->mtx_wantipl) */
1:
	ld	r4,  r31, 0
	st	r2,  r4,  MTX_OLDIPL	/* save into mtx_oldipl */

	ld	r1,  r31, 4
	jmp.n	r1
	 addu	r31, r31, 8

/*
 * void mtx_leave(struct mutex *mtx)
 */
ENTRY(mtx_leave)
	ld	r3,  r2,  MTX_OLDIPL
	ld	r4,  r2,  MTX_WANTIPL
	st	r0,  r2,  MTX_CPU		/* mtx->mtx_cpu = NULL */
	bcnd.n	eq0, r4,  2f
	 st	r0,  r2,  MTX_LOCK		/* mtx->mtx_lock = 0 */

	bcnd	ne0, r3,  1f			/* splx(mtx->mtx_oldipl) */
	br	_C_LABEL(spl0)
1:
	br.n	_C_LABEL(setipl)
	 or	r2,  r3,  r0
2:
	jmp	r1
