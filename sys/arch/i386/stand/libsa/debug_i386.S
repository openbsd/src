/*	$OpenBSD: debug_i386.S,v 1.2 1997/04/04 04:47:47 mickey Exp $	*/

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
#ifndef NO_IDTR

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

alltraps:
#ifdef	DEBUG
	movl	$0xb8280, %edi
	movl	$0x47304731, (%edi)
#endif
	/* lcall is busted */
	pushl	$0x8
	pushl	$1f
	ljmp	$0x8, $_check_regs
1:
	addl	$0x8, %esp
#ifdef	DEBUG
	movl	$0xb8290, %edi
	movl	$0x47384739, (%edi)
#endif
	iret

#endif

	.text

ENTRY(check_regs)
#ifdef	DEBUG
	movl	$0xb8284, %edi
	movl	$0x47324733, (%edi)
#endif
	pushal		# 8 ones
	pushl	%ds
	pushl	%es
	pushl	%fs
	pushl	%gs
	movl	$0x10, %eax
	movl	%ax, %ds
	movl	$_reg, %edi
	cld
	movl	0x0b*4(%esp), %eax; stosl # %eax
	movl	0x0a*4(%esp), %eax; stosl # %ecx
	movl	0x09*4(%esp), %eax; stosl # %edx
	movl	0x08*4(%esp), %eax; stosl # %ebx
	movl	0x07*4(%esp), %eax; stosl # %esp
	movl	0x06*4(%esp), %eax; stosl # %ebp
	movl	0x05*4(%esp), %eax; stosl # %esi
	movl	0x04*4(%esp), %eax; stosl # %edi
	movl	0x10*4(%esp), %eax; stosl # %eip
	movl	0x12*4(%esp), %eax; stosl # %eflags
	movl	0x11*4(%esp), %eax; stosl # %cs
	movl		%ss,  %eax; stosl # %ss
	movl	0x03*4(%esp), %eax; stosl # %ds
	movl	0x02*4(%esp), %eax; stosl # %es
	movl	0x01*4(%esp), %eax; stosl # %fs
	movl	0x00*4(%esp), %eax; stosl # %gs

#ifdef	DEBUG
	movl	$0xb8288, %edi
	movl	$0x47344735, (%edi)
#endif
	call	_C_LABEL(dump_regs)

#ifdef	DEBUG
	movl	$0xb828c, %edi
	movl	$0x47364737, (%edi)
#endif
	popl	%gs
	popl	%fs
	popl	%es
	popl	%ds
	popal
	lret


