/*	$NetBSD: asm.h,v 1.3 1995/05/03 19:53:40 ragge Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)asm.h	5.5 (Berkeley) 5/7/91
 *      @(#)DEFS.h      5.3 (Berkeley) 6/1/90
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

#define R0      0x001
#define R1      0x002
#define R2      0x004
#define R3      0x008
#define R4      0x010
#define R5      0x020
#define R6      0x040
#define R7      0x080
#define R8      0x100
#define R9      0x200
#define R10     0x400
#define R11     0x800

#ifdef __STDC__
# define _FUNC(x)       _ ## x ## :
# define _GLOB(x)       .globl _ ## x
#else
# define _FUNC(x)       _/**/x:
# define _GLOB(x)        .globl _/**/x
#endif

#ifdef PROF
#define ENTRY(x,regs) \
        _GLOB(x);.align 2;_FUNC(x);.word regs;jsb mcount;
#else   
#define ENTRY(x,regs) \
        _GLOB(x);.align 2;_FUNC(x);.word regs;
#endif

#define	ASMSTR		.asciz

#endif /* !_MACHINE_ASM_H_ */
