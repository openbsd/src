/*	$OpenBSD: gidt.S,v 1.1 1997/04/05 18:56:27 mickey Exp $	*/

/*
 * Copyright (c) 1997 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR 
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED 
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <machine/asm.h>
#include <machine/psl.h>
#define _LOCORE
#include <machine/segments.h>
#include <machine/trap.h>
#include <debug_md.h>
#undef _LOCORE


	.data
	.globl	_Gdtr
	.globl	_codeseg

	.align	3
gdt:
		/* 0x00 : null */
	.space	8
		/* 0x08 : flat code */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMERA | 0 | 0x80	# RWXA, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
		/* 0x10 : flat data */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMRWA | 0 | 0x80	# RWA, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
		/* 0x18 : 16 bit code */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMERA | 0 | 0x80	# RWXA, dpl = 0, present
	.byte	0xf | 0 | 0 | 0		# hilimit, xx, 16bit, byte granularity
	.byte	0			# hibase
_Gdtr:	.word	. - gdt - 1
	.long	gdt
	.word	0

_codeseg:	.long	0

	.globl	_Idtr_real
	.align 3
_Idtr_real:
	.word	1023
	.long	0

	.globl	_Idtr_prot

	.align 3
idt:	/*
	 * We beleive that all the boot code fits into
	 * 64k, so no need for high 16 bit of procedure address (;
	 *
	 */
#define IPROC(n)	X/**/n
#define IDTENTRY(proc) \
	.word	IPROC(proc)		/* lo offset handler */ ; \
	.word	0x8			/* handler %cs */ ; \
	.byte	0			/* reserved */ ; \
	.byte	0x80 | SDT_SYS386TGT /* present, dpl=0, 32bit trap gate */ ; \
	.word	0			/* hi offset handler */

	IDTENTRY(de)	/* #DE divide by zero */
	IDTENTRY(db)	/* #DB debug */
	IDTENTRY(nmi)	/* NMI */
	IDTENTRY(bp)	/* #BP breakpoint */
	IDTENTRY(of)	/* #OF overflow */
	IDTENTRY(br)	/* #BR BOUND range exceeded */
	IDTENTRY(ud)	/* #UD invalid opcode */
	IDTENTRY(nm)	/* #NM device not available */
	IDTENTRY(df)	/* #DF double fault */
	IDTENTRY(fo)	/* #FO coprocessor segment overrun */
	IDTENTRY(ts)	/* #TS innvalid TSS */
	IDTENTRY(np)	/* #NP segmant not present */
	IDTENTRY(ss)	/* #SS stack fault */
	IDTENTRY(gp)	/* #GP general protection */
	IDTENTRY(pf)	/* #PF page fault */
	IDTENTRY(xx)	/*	Intel reserved */
	IDTENTRY(mf)	/* #MF floating point error */
	IDTENTRY(ac)	/* #AC alignment check */
	IDTENTRY(mc)	/* #MC machine check */
			/*	Intel reserved (19-31) */
	IDTENTRY(xx); IDTENTRY(xx); IDTENTRY(xx); IDTENTRY(xx)
	IDTENTRY(xx); IDTENTRY(xx); IDTENTRY(xx); IDTENTRY(xx)
	IDTENTRY(xx); IDTENTRY(xx); IDTENTRY(xx); IDTENTRY(xx)
	IDTENTRY(xx)
			/*	Maskable interrupts(32-255) */

_Idtr_prot:
	.word	. - idt
	.long	idt

	.text
	.globl	alltraps

#define IENTRY(name,type) \
IPROC(name): \
	pushl	$type ; \
	jmp	alltraps
#define IENTRY_ERR(name,err,type) \
IPROC(name): \
	pushl	$err ; \
	pushl	$type ; \
	jmp	alltraps

IPROC(xx):
	pushl	$1
	pushl	$256
	jmp	alltraps

IENTRY_ERR(de,0,T_DIVIDE)
IENTRY_ERR(db,0,T_TRCTRAP)
IENTRY_ERR(nmi,0,T_NMI)
IENTRY_ERR(bp,0,T_BPTFLT)
IENTRY_ERR(of,0,T_OFLOW)
IENTRY_ERR(br,0,T_BOUND)
IENTRY_ERR(ud,0,T_PRIVINFLT)
IENTRY_ERR(nm,0,T_DNA)
IENTRY(df,T_DOUBLEFLT)
IENTRY_ERR(fo,0,T_FPOPFLT)
IENTRY(ts,T_TSSFLT)
IENTRY(np,T_SEGNPFLT)
IENTRY(ss,T_STKFLT)
IENTRY(gp,T_PROTFLT)
IENTRY(pf,T_PAGEFLT)
IENTRY_ERR(mf,0,T_ARITHTRAP)
IENTRY(ac,T_ALIGNFLT)
IENTRY(mc,T_MACHK)

