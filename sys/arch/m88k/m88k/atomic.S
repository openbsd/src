/*	$OpenBSD: atomic.S,v 1.2 2009/02/20 23:35:32 miod Exp $	*/

/*
 * Copyright (c) 2009 Miodrag Vallat.
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

#include <machine/asm.h>

	.data

/*
 * A __cpu_simplelock_t used to provide the inter-processor interlock.
 */
ASLOCAL(__atomic_interlock)
	.word	0

	.text

/*
 * Register usage in this file:
 *
 * r2 data address
 * r3 bits to set or clear
 * r4 return address
 * r5 scratch
 * r6 interlock address
 * r7 psr upon entry
 * r8 active psr
 */

ENTRY(atomic_setbits_int)
	or	r4, r1, r0		/* save return address */
	bsr	_C_LABEL(__atomic_lock)

	ld	r5, r2, r0
	or	r5, r5, r3
	st	r5, r2, r0

	br	_C_LABEL(__atomic_unlock)

ENTRY(atomic_clearbits_int)
	or	r4, r1, r0		/* save return address */
	bsr	_C_LABEL(__atomic_lock)

	ld	r5, r2, r0
	or	r5, r5, r3
	xor	r5, r5, r3		/* r5 &= ~r3 */
	st	r5, r2, r0

	br	_C_LABEL(__atomic_unlock)

GLOBAL(__atomic_lock)

/*
 * If running a kernel with support for both 88100 and 88110 compiled-in
 * on a 88100 machine, the 88100 code (shorter) will be copied over in
 * vector_init().
 */

#ifdef M88110
ASLOCAL(__atomic_lock_88110)
	/*
	 * This is the 88110 version: disable shadowing and interrupts,
	 * then grab the interlock.
	 */

	ldcr	r7, PSR
	set	r8, r7, 1<PSR_INTERRUPT_DISABLE_BIT>
	set	r8, r8, 1<PSR_SHADOW_FREEZE_BIT>
	stcr	r8, PSR
	FLUSH_PIPELINE

	or.u	r6, r0, hi16(_ASM_LABEL(__atomic_interlock))
	or	r6, r6, lo16(_ASM_LABEL(__atomic_interlock))
1:
	or	r5, r0, 1	/* __SIMPLELOCK_LOCKED */
	xmem	r5, r6, r0
	bcnd	eq0, r5, 3f
2:
	ld	r5, r6, r0
	bcnd	eq0, r5, 1b
	br	2b
3:
	jmp	r1
#endif

#ifdef M88100
GLOBAL(__atomic_lock_88100)
	/*
	 * This is the 88100 version: disable interrupts, then grab
	 * the interlock.
	 */

	ldcr	r7, PSR
	set	r8, r7, 1<PSR_INTERRUPT_DISABLE_BIT>
	stcr	r8, PSR
	FLUSH_PIPELINE

	or.u	r6, r0, hi16(_ASM_LABEL(__atomic_interlock))
	or	r6, r6, lo16(_ASM_LABEL(__atomic_interlock))
1:
	or	r5, r0, 1	/* __SIMPLELOCK_LOCKED */
	xmem	r5, r6, r0
	bcnd	eq0, r5, 3f
2:
	ld	r5, r6, r0
	bcnd	eq0, r5, 1b
	br	2b
3:
	jmp	r1
GLOBAL(__atomic_lock_88100_end)
#endif

GLOBAL(__atomic_unlock)

/*
 * If running a kernel with support for both 88100 and 88110 compiled-in
 * on a 88100 machine, the 88100 code (shorter) will be copied over in
 * vector_init().
 */

#ifdef M88110
ASLOCAL(__atomic_unlock_88110)
	/*
	 * This is the 88110 version: release the interlock, set up
	 * exception registers to return to our caller with initial
	 * psr restored.
	 */

	st	r0, r6, r0	/* release interlock */

	stcr	r4, EXIP	/* return address */
	stcr	r7, EPSR	/* original PSR */

	/*
	 * No need to workaround errata #18 (see m88110_user_rte in
	 * eh_common.S), as we are not returning to user mode.
	 */
	RTE
#endif

#ifdef M88100
GLOBAL(__atomic_unlock_88100)
	/*
	 * This is the 88100 version: release the interlock,
	 * restore psr and return to the caller.
	 */

	st	r0, r6, r0	/* release interlock */

	stcr	r7, PSR
	FLUSH_PIPELINE
	
	jmp	r4
GLOBAL(__atomic_unlock_88100_end)
#endif
