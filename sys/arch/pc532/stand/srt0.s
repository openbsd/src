/*	$NetBSD: srt0.s,v 1.3 1995/08/29 21:55:51 phil Exp $	*/

/*-
 * Copyright (c) 1994 Philip L. Budne.
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
 *	This product includes software developed by Philip L. Budne.
 * 4. The name of Philip L. Budne may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PHILIP NELSON ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL PHILIP NELSON BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * srt0.s -- standalone C startup
 * Phil Budne, May 10, 1994
 *
 * from: pc532 kernel locore.s 
 * Phil Nelson, Dec 6, 1992
 *
 */

#define PSR_S 0x200
#define PSR_I 0x800

.data
.globl _howto, _bootdev, _r3, _r6, _r7
__save_sp: .long 0
__save_fp: .long 0
_bootdev: .long 0
_howto:	.long 0
_r3: .long 0
_r6: .long 0
_r7: .long 0

.text
.globl begin
begin:
	bicpsrw	PSR_I			/* make sure interrupts are off. */
	bicpsrw	PSR_S			/* make sure we are using sp0. */

	/* In case we are zboot: */
	movd	r3,_r3(pc)		/* magic */
	movd	r6,_r6(pc)		/* devtype */
	movd	r7,_r7(pc)		/* howto */

	lprd    sb, 0			/* gcc expects this. */
	sprd	sp, __save_sp(pc)  	/* save monitor's sp. */
	sprd	fp, __save_fp(pc)  	/* save monitor's fp. */
/*	sprd	intbase, __old_intbase(pc)  /* save monitor's intbase. */
	movqd	0, _howto(pc)

restart:
	/* Zero the bss segment. */
	addr	_end(pc),r0	# setup to zero the bss segment.
	addr	_edata(pc),r1
	subd	r1,r0		# compute _end - _edata
	movd	r0,tos		# push length
	addr	_edata(pc),tos	# push address

	movqd	0,_bootdev(pc)	# XXX trash bootdev
	bsr	_bzero		# zero the bss segment
	bsr	_main
	/* fall */

.globl __rtt
__rtt:	lprd	sp, __save_sp(pc)  	/* restore monitor's sp. */
	lprd	fp, __save_fp(pc)  	/* restore monitor's fp. */
/* XXX just return to monitor??? */
	movqd	3, _howto		/* RB_SINGLE|RB_ASKNAME */
	br	restart
