/*	$NetBSD: cpufunc.h,v 1.1 1996/01/26 05:37:16 phil Exp $	*/

/*
 * Copyright (c) 1996 Matthias Pfaller.
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
 *	This product includes software developed by Matthias Pfaller.
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
 */

#ifndef _NS532_CPUFUNC_H_
#define	_NS532_CPUFUNC_H_

/*
 * Load a mmu register.
 */
#define	lmr(reg, src) __asm __volatile("lmr " #reg ",%0" : : "g" (src))

/*
 * Store a mmu register.
 */
#define	smr(reg, dst) __asm __volatile("smr " #reg ",%0" : "=g" (dst) :)

/*
 * Load the FPU status register.
 */
#define lfsr(src) __asm __volatile("lfsr %0" : : "g" (src))

/*
 * Store the FPU status register.
 */
#define sfsr(src) __asm __volatile("sfsr %0" : "=g" (src) :)

/*
 * Load a processor register.
 */
#define lprd(reg, src) __asm __volatile("lprd " #reg ",%0" : : "g" (src))
#define lprw(reg, src) __asm __volatile("lprw " #reg ",%0" : : "g" (src))
#define lprb(reg, src) __asm __volatile("lprb " #reg ",%0" : : "g" (src))

/*
 * Store a processor register.
 */
#define sprd(reg, dst) __asm __volatile("sprd " #reg ",%0" : "=g" (dst) :)
#define sprw(reg, dst) __asm __volatile("sprw " #reg ",%0" : "=g" ((short) (dst)) :)
#define sprb(reg, dst) __asm __volatile("sprb " #reg ",%0" : "=g" ((char) (dst)) :)

/*
 * Move data. This can be used to force
 * gcc to load a register variable.
 */
#define movd(src, dst) __asm __volatile("movd %1,%0" : "=g" (dst) : "g" (src))

/*
 * Load the ptb. This loads ptb0 and ptb1 to
 * avoid a cpu-bug when using dual address
 * space instructions.
 */
#define load_ptb(src) __asm __volatile("lmr ptb0,%0; lmr ptb1,%0" : : "g" (src))

/*
 * Flush tlb. Just to be save this flushes
 * kernelmode and usermode translations.
 */
#define tlbflush() __asm __volatile("smr ptb0,r0; lmr ptb0,r0; lmr ptb1,r0" : : : "r0")

/*
 * Bits in the cfg register.
 */
#define CFG_I	0x0001		/* Enable vectored interrupts */
#define CFG_F	0x0002		/* Enable floating-point instruction set */
#define CFG_M	0x0004		/* Enable memory management instruction set */
#define CFG_ONE	0x00f0		/* Must be one */
#define CFG_DE	0x0100		/* Enable direct exception mode */
#define CFG_DC	0x0200		/* Enable data cache */
#define CFG_LDC	0x0400		/* Lock data cache */
#define CFG_IC	0x0800		/* Enable instruction cache */
#define CFG_LIC	0x1000		/* Lock instruction cache */
#define	CFG_PF	0x2000		/* Enable pipelined floating-point execution */

#endif /* !_NS532_CPUFUNC_H_ */
