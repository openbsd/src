/*	$OpenBSD: gidt.S,v 1.5 1997/05/29 05:32:07 mickey Exp $	*/

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

	.file "gidt.S"

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

	.text
	.globl	pmm_init

pmm_init:
	/* reload new gdt */
	lgdt	_Gdtr
	ljmp	$8, $1f
1:	
	movw	$0x10, %ax
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%ax, %es
	movl	%ax, %fs
	movl	%ax, %gs
	/* init idt */
	call	idt_init
	/* load idtr for interrupts */
	lidt	_Idtr_prot
	ret

	.data

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
	.word	(START & 0xffff)	# lobase
	.byte	(START >> 16) & 0xff	# midbase
	.byte	SDT_MEMERA | 0 | 0x80	# RXAC, dpl = 0, present
	.byte	0xf | 0 | 0 | 0		# hilimit, xx, 16bit, byte granularity
	.byte	(START >> 20) & 0xff	# hibase
		/* 0x20 : 16 bit data */
	.word	0xFFFF			# lolimit
	.word	(START & 0xffff)	# lobase
	.byte	(START >> 16) & 0xff	# midbase
	.byte	SDT_MEMRWA | 0 | 0x80	# RXAC, dpl = 0, present
	.byte	0xf | 0 | 0 | 0		# hilimit, xx, 16bit, byte granularity
	.byte	(START >> 20) & 0xff	# hibase

_Gdtr:	.word	. - gdt - 1
	.long	gdt
	.word	0

	.align 3
_Idtr_real:
	.word	1023
	.long	0

	.align 3
idt:
	.space 32*8, 0	/* internal (0-31) */
			/*	Maskable interrupts (32-255) */
	.space 32*8, 0	/* BIOS entry points (32-63) */
	.space 16*8, 0	/* DOS entry points (64-80) */

_Idtr_prot:
	.word	. - idt - 1
	.long	idt

	.text
#define IPROC(n)	X/**/n
#define IBIOS(n)	IPROC(bios/**/n)
#define IDOS(n)		IPROC(dos/**/n)

idt_init:
	pushl	%edi

	movl	$idt, %edi

#define	fixe(p)								\
	movl	$ IPROC(p), %eax;					\
	stosw; 					/* lo offset handler */	\
	movw	$0x8, (%edi);			/* handler %cs */	\
	incl %edi; incl %edi;						\
	incl %edi;				/* reserved */		\
	movb	$(0x80|SDT_SYS386TGT), (%edi);	/* 32bit trap gate */	\
	incl %edi;							\
	shrl	$16, %eax;						\
	stosw;					/* hi offset handler */

	fixe(de); fixe(db); fixe(nmi); fixe(bp); fixe(of); fixe(br)
	fixe(ud); fixe(nm); fixe(df);  fixe(fo); fixe(ts); fixe(np)
	fixe(ss); fixe(gp); fixe(pf);  fixe(xx); fixe(mf); fixe(ac)
	fixe(mc)
	fixe(xx); fixe(xx); fixe(xx);  fixe(xx); fixe(xx); fixe(xx)
	fixe(xx); fixe(xx); fixe(xx);  fixe(xx); fixe(xx); fixe(xx)
	fixe(xx) 

#define fixb(b) fixe(bios/**/b)
	fixb(0);  fixb(1);  fixb(2);  fixb(3);  fixb(4);  fixb(5)
	fixb(6);  fixb(7);  fixb(8);  fixb(9);  fixb(10); fixb(11)
	fixb(12); fixb(13); fixb(14); fixb(15); fixb(16); fixb(17)
	fixb(18); fixb(19); fixb(20); fixb(21); fixb(22); fixb(23)
	fixb(24); fixb(25); fixb(26); fixb(27); fixb(28); fixb(29)
	fixb(30); fixb(31)

#define fixd(d) fixe(dos/**/b)

	popl	%edi
	ret

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
	pushl	$T_RESERVED
	jmp	1f

IENTRY_ERR(de,0,T_DIVIDE)	/* #DE divide by zero */
IENTRY_ERR(db,0,T_TRCTRAP)	/* #DB debug */
IENTRY_ERR(nmi,0,T_NMI)		/* NMI */
IENTRY_ERR(bp,0,T_BPTFLT)	/* #BP breakpoint */
IENTRY_ERR(of,0,T_OFLOW)	/* #OF overflow */
IENTRY_ERR(br,0,T_BOUND)	/* #BR BOUND range exceeded */
IENTRY_ERR(ud,0,T_PRIVINFLT)	/* #UD invalid opcode */
IENTRY_ERR(nm,0,T_DNA)		/* #NM device not available */
IENTRY(df,T_DOUBLEFLT)		/* #DF double fault */
IENTRY_ERR(fo,0,T_FPOPFLT)	/* #FO coprocessor segment overrun */
IENTRY(ts,T_TSSFLT)		/* #TS innvalid TSS */
IENTRY(np,T_SEGNPFLT)		/* #NP segmant not present */
IENTRY(ss,T_STKFLT)		/* #SS stack fault */
IENTRY(gp,T_PROTFLT)		/* #GP general protection */
IENTRY(pf,T_PAGEFLT)		/* #PF page fault */
IENTRY_ERR(mf,0,T_ARITHTRAP)	/* #MF floating point error */
IENTRY(ac,T_ALIGNFLT)		/* #AC alignment check */
IENTRY(mc,T_MACHK)		/* #MC machine check */

	.globl	alltraps
1:	/* save on jumps */
	jmp	alltraps

#define	IBIOSENT(n)	IBIOS(n): pushl $n; jmp BIOSh

IBIOSENT(0);  IBIOSENT(1);  IBIOSENT(2);  IBIOSENT(3)
IBIOSENT(4);  IBIOSENT(5);  IBIOSENT(6);  IBIOSENT(7)
IBIOSENT(8);  IBIOSENT(9);  IBIOSENT(10); IBIOSENT(11)
IBIOSENT(12); IBIOSENT(13); IBIOSENT(14); IBIOSENT(15)
IBIOSENT(16); IBIOSENT(17); IBIOSENT(18); IBIOSENT(19)
IBIOSENT(20); IBIOSENT(21); IBIOSENT(22); IBIOSENT(23)
IBIOSENT(24); IBIOSENT(25); IBIOSENT(26); IBIOSENT(27)
IBIOSENT(28); IBIOSENT(29); IBIOSENT(30); IBIOSENT(31)

/*
 * entry point for BIOS real-mode interface
 * all the magic for real-prot mode switching is here
 *
 * Call:	%eax, %ecx, %edx, %ebx, %ebp, %esi, %ds, %di==real mode %es
 * Return:	%eax, %edx, %ecx, %eflags (as returned from BIOS)
 *
 */
	.globl	BIOSh
BIOSh:
	pushal
	pushl	%ds
	pushl	%es

	/* save %eax */
	movl	%eax, 2f

	/* save BIOS int vector */
	movb	10*4(%esp), %al
	movb	%al, intno

	/* setup 16bit labels in ljmps */

#ifdef	GIDT_DEBUG
	movl	$0xb8000, %eax
	movl	$0x47314730, (%eax)
#endif
	movl	$0x20, %eax

	# ljmp	$0x18, $2f
	.byte	0xea		/* Change to 16bit mode */
	.long	1f - START
	.word	0x18
1:
	movl	%ax, %ds
#ifdef	GIDT_DEBUG
	data32
	movl	$(0xb8000 - START), %eax
	data32
	addr32
	movl	$0x4f314f30, (%eax)
#endif
	movl	%cr0, %eax	/* disable prot mode memory management */
	data32
	andl 	$~CR0_PE, %eax
	movl	%eax, %cr0

	# ljmp	(START >> 16), $4f
	.byte	0xea		/* load real mode cs:ip */
	.word	1f
	.word	(START >> 4)
1:
	xorl	%eax, %eax	/* setup: %ds, %es, %ss */
	movl	%ax, %ds
	movl	%ax, %ss
	movl	%di, %es
#ifdef	GIDT_DEBUG
	data32
	movl	$0xb8004, %eax
	data32
	addr32
	movl	$0x47334732, (%eax)
#endif

	addr32
	data32
	lidt	_Idtr_real	/* load idtr for DOS/BIOS operations */

	data32
	# movl	Leax, %eax
	.byte	0xb8
2:	.long	0x90909090	/* restore %eax */

	sti

	int	$0
intno	= . - 1

	movb	%ah, %bh	/* save flags to return to caller */
	lahf
	xchgb	%ah, %bh

	cli			/* no maskable interrupts in prot mode */

	addr32
	data32
	movl	%eax, 2f	/* save %eax */

#ifdef	GIDT_DEBUG
	data32
	movl	$0xb8004, %eax
	data32
	addr32
	movl	$0x4f334f32, (%eax)
#endif

	addr32
	data32
	lgdt	_C_LABEL(Gdtr)	/* load the gdtr */

	movl	%cr0, %eax	/* enable prot mode memory management */
	data32
	orl	$CR0_PE, %eax
	movl	%eax, %cr0 

	data32
	ljmp	$0x08, $1f	/* reload %cs, and flush pipeline */
1:
	/* reload 32bit %ds, %ss, %es */
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	%ax, %ss

#ifdef	GIDT_DEBUG
	movl	$0xb8008, %eax
	movl	$0x47344733, (%eax)
#endif

	/* load idtr for debugger and DOS/BIOS iface */
	lidt	_Idtr_prot

	# movl	Leax, %eax
	.byte	0xb8
2:	.long	0x90909090	/* eax */

	/* pass BIOS return values back to caller */
	movl	%eax, 0x9*4(%esp)
	movl	%ecx, 0x8*4(%esp)
	movl	%edx, 0x7*4(%esp)
	movb	%bh , 0xd*4(%esp)

	popl	%es
	popl	%ds
	popal
	incl %esp; incl %esp; incl %esp; incl %esp
	iret

