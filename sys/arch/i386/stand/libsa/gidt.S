/*	$OpenBSD: gidt.S,v 1.3 1997/04/09 08:39:37 mickey Exp $	*/

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
#include <machine/specialreg.h>
#include <machine/trap.h>
#include <debug_md.h>
#undef _LOCORE

#define addr32  .byte 0x67
#define data32  .byte 0x66

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
	.byte	SDT_MEMERAC | 0 | 0x80	# RXAC, dpl = 0, present
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
	.byte	SDT_MEMERA | 0 | 0x80	# RXAC, dpl = 0, present
	.byte	0x0 | 0 | 0 | 0		# hilimit, xx, 16bit, byte granularity
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

	.globl	_Idtr_reset
	.align 3
_Idtr_reset:
	.word	0
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
			/*	BIOS entry points (32-63) */
#define IBIOS(n)	IPROC(bios/**/n)
#define IDTBIOS(n)	IDTENTRY(bios/**/n)
	IDTBIOS(0);  IDTBIOS(1);  IDTBIOS(2);  IDTBIOS(3);  IDTBIOS(4)
	IDTBIOS(5);  IDTBIOS(6);  IDTBIOS(7);  IDTBIOS(8);  IDTBIOS(9)
	IDTBIOS(10); IDTBIOS(11); IDTBIOS(12); IDTBIOS(13); IDTBIOS(14)
	IDTBIOS(15); IDTBIOS(16); IDTBIOS(17); IDTBIOS(18); IDTBIOS(19)
	IDTBIOS(20); IDTBIOS(21); IDTBIOS(22); IDTBIOS(23)
			/*	DOS entry points */

_Idtr_prot:
	.word	. - idt - 1
	.long	idt

	.text

#define IENTRY(name,type) \
IPROC(name): \
	pushl	$type ; \
	jmp	1f
#define IENTRY_ERR(name,err,type) \
IPROC(name): \
	pushl	$err ; \
	pushl	$type ; \
	jmp	1f

IPROC(xx):
	pushl	$1
	pushl	$256
	jmp	1f

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

	.globl	alltraps
1:	/* save on jumps */
	jmp	alltraps

#define	IBIOSENT(n)	IBIOS(n): pushl %eax; movb $n, %al ; jmp 1f

IBIOSENT(0);  IBIOSENT(1);  IBIOSENT(2);  IBIOSENT(3)
IBIOSENT(4);  IBIOSENT(5);  IBIOSENT(6);  IBIOSENT(7)
IBIOSENT(8);  IBIOSENT(9);  IBIOSENT(10); IBIOSENT(11)
IBIOSENT(12); IBIOSENT(13); IBIOSENT(14); IBIOSENT(15)
IBIOSENT(16); IBIOSENT(17); IBIOSENT(18); IBIOSENT(19)
IBIOSENT(20); IBIOSENT(21); IBIOSENT(22); IBIOSENT(23)

	.text
	.globl	_real_to_prot, _prot_to_real
1:
	movb	%al, intno
	popl	%eax
	pushal
	pushl	%ds
	pushl	%es

	call	_prot_to_real

	movl	%di, %es

	int	$0
intno = . - 1

	movb	%ah, %bh
	lahf
	xchgb	%ah, %bh

	data32
	call	_real_to_prot

	/* pass BIOS return values back to caller */
	movl	%eax, 0x9*4(%esp)
	movl	%ecx, 0x8*4(%esp)
	movl	%edx, 0x7*4(%esp)
	movb	%bh , 0xc*4(%esp)

	popl	%es
	popl	%ds
	popal
	iret

