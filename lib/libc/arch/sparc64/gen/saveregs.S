/*	$OpenBSD: saveregs.S,v 1.2 2003/06/02 20:18:32 millert Exp $	*/
/*	$NetBSD: saveregs.S,v 1.2 2000/07/21 03:14:15 eeh Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 * from: Header: saveregs.s,v 1.1 91/07/06 17:22:33 torek Exp
 */

#include <machine/asm.h>

/*
 * Save register arguments in caller's `arg dump' area, so that
 * stdarg functions work.
 *
 * This really should be done with a pointer to the arg dump area;
 * our caller should allocate that area, not our caller's caller.
 * But then, they did not let me invent the calling sequence....
 *
 * We assume the caller has executed a `save' instruction.
 */
#define	REGOFF	(BIAS - CC64FSZ)
	
ENTRY(__builtin_saveregs)
	stx	%i0, [%fp + REGOFF + 0x00]	! fr->fr_argd[0]
	stx	%i1, [%fp + REGOFF + 0x08]	! fr->fr_argd[1]
	stx	%i2, [%fp + REGOFF + 0x10]	! fr->fr_argd[2]
	stx	%i3, [%fp + REGOFF + 0x18]	! fr->fr_argd[3]
	stx	%i4, [%fp + REGOFF + 0x20]	! fr->fr_argd[4]
	retl
	 stx	%i5, [%fp + REGOFF + 0x28]	! fr->fr_argd[5]
