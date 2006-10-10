/*	$OpenBSD: sigprocmask.S,v 1.1.1.1 2006/10/10 22:07:10 miod Exp $	*/
/*	$NetBSD: sigprocmask.S,v 1.6 2003/08/07 16:42:21 agc Exp $	*/

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)sigprocmask.s	5.2 (Berkeley) 12/17/90
 */

#include "SYS.h"

SYSENTRY(sigprocmask)
	mov	r5, r2			/* fetch new sigset pointer */
	tst	r2, r2			/* check new sigset pointer */
	bf	1f			/* if not null, indirect */
	mov	#1, r4			/* SIG_BLOCK */
	bra	2f
	nop
1:	mov.l	@r2, r2			/* fetch indirect ... */
	mov	r2, r5			/* to new mask arg */
2:	mov.l	LSYS_sigprocmask, r0
	trapa	#0x80
	bf	err
	mov	r6, r2			/* fetch old mask requested */
	tst	r2, r2			/* test if old mask requested */
	bt	out
	mov.l	r0, @r2			/* store old mask */
out:
	xor	r0, r0
	rts
	nop
err:
	JUMP_CERROR

	.align	2
LSYS_sigprocmask:
	.long	SYS_sigprocmask
