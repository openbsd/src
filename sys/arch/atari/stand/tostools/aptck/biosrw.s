/*	$NetBSD: biosrw.s,v 1.1.1.1 1996/01/07 21:54:15 leo Exp $	*/

/*
 * Copyright (c) 1995 Waldi Ravens.
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
 *        This product includes software developed by Waldi Ravens.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/* int	bios_read(buffer, offset, count, dev)	*/

	.globl	_bios_read
	.text
	.even

_bios_read:
	movml	d1-d2/a1-a2,sp@-
	movl	sp@(24),sp@-		| offset
	movw	sp@(38),sp@-		| device
	movw	#-1,sp@-
	movw	sp@(38),sp@-		| count
	movl	sp@(30),sp@-		| buffer
	movw	#8,sp@-			| read, physical mode
	movw	#4,sp@-
	trap	#13			| Rwabs()
	lea	sp@(18),sp
	movml	sp@+,d1-d2/a1-a2
	rts

/* int	bios_write(buffer, offset, count, dev)	*/

	.globl	_bios_write
	.text
	.even

_bios_write:
	movml	d1-d2/a1-a2,sp@-
	movl	sp@(20),sp@-		| offset
	movw	sp@(34),sp@-		| device
	movw	#-1,sp@-
	movw	sp@(34),sp@-		| count
	movl	sp@(26),sp@-		| buffer
	movw	#9,sp@-			| write, physical mode
	movw	#4,sp@-
	trap	#13			| Rwabs()
	lea	sp@(18),sp
	movml	sp@+,d1-d2/a1-a2
	rts

/* int	bios_critic(error)			*/

	.globl	_bios_critic
	.text
	.even

_bios_critic:
	movw	sp@(4),d0
	extl	d0
	rts
