/*
 * $OpenBSD: printf.s,v 1.1 1997/01/16 09:26:40 niklas Exp $
 * $NetBSD: printf.s,v 1.1.1.1 1996/11/29 23:36:29 is Exp $
 *
 *
 * Copyright (c) 1996 Ignatios Souvatzis
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
 *      This product includes software developed by Ignatios Souvatzis
 *      for the NetBSD project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*
 * printf calling exec's RawDoFmt
 * Beware! You have to explicitly use %ld etc. for 32bit integers!
 */
	.text
	.globl	_printf
	.globl	_putchar, _SysBase

Lputch:
	movl	d0,sp@-
	bsr	_putchar
	addql	#4,sp
	rts

_printf:
	movml	#0x0032,sp@-
	lea	pc@(Lputch:w),a2
	lea	sp@(20),a1
	movl	sp@(16),a0
	movl	pc@(_SysBase:w),a6
	jsr	a6@(-0x20a)
	movml	sp@+, #0x4c00
	rts
#if 0
Lstorech:
	movb	d0, a3@+
	rts

	.globl _sprintf
_sprintf:
	movml	#0x0032,sp@-
	movl	sp@(16),a3
	lea	pc@(Lstorech:w),a2
	lea	sp@(24),a1
	movl	sp@(20),a0
	movl	pc@(_SysBase:w),a6
	jsr	a6@(-0x20a)
	movml	sp@+, #0x4c00
	rts
#endif
