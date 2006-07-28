/*	$OpenBSD: start.S,v 1.1 2006/07/28 17:12:06 kettenis Exp $	*/
/*	$NetBSD: srtbegin.S,v 1.7 2005/12/11 12:17:10 christos Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <machine/asm.h>
#include <arm/armreg.h>

#define	STACKSIZE	8192

ENTRY(start)
	/*
	 * We assume we've been loaded VA==PA, or that the MMU
	 * is disabled.  Make sure the MMU is disabled so that
	 * we don't have to care about the caches.
	 */
	/* Clear the BSS. */
	adr	r1, Lbss
	ldmia	r1, {r1, r2}
	sub	r2, r2, r1
	mov	r3, #0

1:	strb	r3, [r1], #0x01
	subs	r2, r2, #0x01
	bgt	1b

	/* Set the stack pointer */
	adr	r1, Lstack
	ldr	r1, [r1]
	add	sp, r1, #STACKSIZE

	b	_C_LABEL(main)

Ltext:
	.word	_C_LABEL(__text_store)
	.word	_C_LABEL(start)
	.word	_C_LABEL(_etext)

Ldata:
	.word	_C_LABEL(__data_store)
	.word	_C_LABEL(__data_start)

Lbss:
	.word	_C_LABEL(_edata)
	.word	_C_LABEL(_end)

Lstack:
	.word	Lstackspace

	.comm	Lstackspace, STACKSIZE
