/*	$OpenBSD: muldi3.s,v 1.1 1997/01/16 09:26:39 niklas Exp $
/*	$NetBSD: muldi3.s,v 1.1.1.1 1996/11/29 23:36:30 is Exp $
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
 * Slow but small muldi3.
 * Beware... the multiplicand is unsigned (but should be enough for
 * usage by ufs.c :-)
 */

	.text
	.even
	.globl	___muldi3
___muldi3:
	movml d2/d3/d4/d5/d6,sp@-	| 0..4 regs, 5 pc, 6..9 parameters
	movml sp@(24),d2-d5
|	movl sp@(24),d2
|	movl sp@(28),d3
|	movl sp@(32),d4
|	movl sp@(36),d5
	movq #0,d0
	movq #0,d1
	movq #63,d6
L4:
	asrl #1,d2
	roxrl #1,d3
	jcc L5
	addl d5,d1
	addxl d4,d0
L5:
	addl d5,d5
	addxl d4,d4
L7:
	dbra d6,L4
	movml sp@+,d2/d3/d4/d5/d6
	rts
