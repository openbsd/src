/*	$NetBSD: intvec.s,v 1.12 1995/11/10 19:05:46 ragge Exp $   */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
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
 *     This product includes software developed at Ludd, University of Lule}.
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

 /* All bugs are subject to removal without further notice */
		


#include "machine/mtpr.h"
#include "machine/pte.h"
#include "machine/trap.h"

#define	TRAPCALL(namn, typ)	\
	.align 2; namn ## :;.globl namn ;pushl $0; pushl $typ; jbr trap;

#define	TRAPARGC(namn, typ)	\
	.align 2; namn ## :;.globl namn ; pushl $typ; jbr trap;

#define	FASTINTR(namn, rutin)	\
	.align 2; namn ## :;.globl namn ;pushr $0x3f; \
	calls $0,_ ## rutin ;popr $0x3f;rei
#define	STRAY(scbnr,vecnr) \
	.align 2;stray ## vecnr ## :;pushr $0x3f;pushl $ ## 0x ## vecnr; \
	pushl $scbnr; calls $2,_stray ; popr $0x3f; rei;
#define	KSTACK 0
#define ISTACK 1
#define INTVEC(label,stack)	\
	.long	label+stack;
		.text

	.globl	_kernbase,_rpb
_kernbase:
_rpb:	
/*
 * First page in memory we have rpb; so that we know where
 * (must be on a 64k page boundary, easiest here). We use it
 * to store SCB vectors generated when compiling the kernel,
 * and move the SCB later to somewhere else.
 */

	INTVEC(stray00, ISTACK)	# Unused., 0
	INTVEC(mcheck, ISTACK)		# Machine Check., 4
	INTVEC(invkstk, ISTACK)	# Kernel Stack Invalid., 8
	INTVEC(stray0C, ISTACK)	# Power Failed., C
	INTVEC(privinflt, KSTACK)	# Privileged/Reserved Instruction.
	INTVEC(stray14, ISTACK)	# Customer Reserved Instruction, 14
	INTVEC(resopflt, KSTACK)	# Reserved Operand/Boot Vector(?), 18
	INTVEC(resadflt, KSTACK)	# # Reserved Address Mode., 1C
	INTVEC(access_v, KSTACK)	# Access Control Violation, 20
	INTVEC(transl_v, KSTACK)	# Translation Invalid, 24
	INTVEC(tracep, KSTACK)	# Trace Pending, 28
	INTVEC(breakp, KSTACK)	# Breakpoint Instruction, 2C
	INTVEC(stray30, ISTACK)	# Compatibility Exception, 30
	INTVEC(arithflt, KSTACK)	# Arithmetic Fault, 34
	INTVEC(stray38, ISTACK)	# Unused, 38
	INTVEC(stray3C, ISTACK)	# Unused, 3C
	INTVEC(syscall, KSTACK)		# main syscall trap, chmk, 40
	INTVEC(resopflt, KSTACK)	# chme, 44
	INTVEC(resopflt, KSTACK)	# chms, 48
	INTVEC(resopflt, KSTACK)	# chmu, 4C
	INTVEC(stray50, ISTACK)	# System Backplane Exception, 50
	INTVEC(cmrerr, ISTACK)	# Corrected Memory Read, 54
	INTVEC(stray58, ISTACK)	# System Backplane Alert, 58
	INTVEC(stray5C, ISTACK)	# System Backplane Fault, 5C
	INTVEC(stray60, ISTACK)	# Memory Write Timeout, 60
	INTVEC(stray64, ISTACK)	# Unused, 64
	INTVEC(stray68, ISTACK)	# Unused, 68
	INTVEC(stray6C, ISTACK)	# Unused, 6C
	INTVEC(stray70, ISTACK)	# Unused, 70
	INTVEC(stray74, ISTACK)	# Unused, 74
	INTVEC(stray78, ISTACK)	# Unused, 78
	INTVEC(stray7C, ISTACK)	# Unused, 7C
	INTVEC(stray80, ISTACK)	# Unused, 80
	INTVEC(stray84, ISTACK)	# Unused, 84
	INTVEC(astintr,	 KSTACK)	# Asynchronous Sustem Trap, AST
	INTVEC(stray8C, ISTACK)	# Unused, 8C
	INTVEC(stray90, ISTACK)	# Unused, 90
	INTVEC(stray94, ISTACK)	# Unused, 94
	INTVEC(stray98, ISTACK)	# Unused, 98
	INTVEC(stray9C, ISTACK)	# Unused, 9C
	INTVEC(softclock,ISTACK)	# Software clock interrupt
	INTVEC(strayA4, ISTACK)	# Unused, A4
	INTVEC(strayA8, ISTACK)	# Unused, A8
	INTVEC(strayAC, ISTACK)	# Unused, AC
	INTVEC(netint,   ISTACK)	# Network interrupt
	INTVEC(strayB4, ISTACK)	# Unused, B4
	INTVEC(strayB8, ISTACK)	# Unused, B8
	INTVEC(ddbtrap, ISTACK)	# Kernel debugger trap, BC
	INTVEC(hardclock,ISTACK)	# Interval Timer
	INTVEC(strayC4, ISTACK)	# Unused, C4
	INTVEC(emulate, KSTACK) # Subset instruction emulation
	INTVEC(strayCC, ISTACK)	# Unused, CC
	INTVEC(strayD0, ISTACK)	# Unused, D0
	INTVEC(strayD4, ISTACK)	# Unused, D4
	INTVEC(strayD8, ISTACK)	# Unused, D8
	INTVEC(strayDC, ISTACK)	# Unused, DC
	INTVEC(strayE0, ISTACK)	# Unused, E0
	INTVEC(strayE4, ISTACK)	# Unused, E4
	INTVEC(strayE8, ISTACK)	# Unused, E8
	INTVEC(strayEC, ISTACK)	# Unused, EC
#ifdef VAX750
	INTVEC(cstrint, ISTACK)	# Console Storage Recieve Interrupt
	INTVEC(csttint, ISTACK)	# Console Storage Transmit Interrupt
#else
	INTVEC(strayF0, ISTACK)
	INTVEC(strayF4, ISTACK)
#endif
	INTVEC(consrint, ISTACK)	# Console Terminal Recieve Interrupt
	INTVEC(constint, ISTACK)	# Console Terminal Transmit Interrupt

	/* space for adapter vectors */
	.space 0x100

	STRAY(0, 00)

		.align 2
#
# mcheck is the badaddress trap, also called when referencing
# a invalid address (busserror)
# _memtest (memtest in C) holds the address to continue execution
# at when returning from a intentional test.
#
mcheck:	.globl	mcheck
	tstl	_cold		# Ar we still in coldstart?
	bneq	L4		# Yes.

	pushr	$0x3f
	pushab	24(sp)
	calls	$1, _machinecheck
	popr	$0x3f
	addl2	(sp)+,sp

        rei

L4:	addl2	(sp)+,sp	# remove info pushed on stack
	mtpr	$0xF,$PR_MCESR	# clear the bus error bit
	movl	_memtest,(sp)	# REI to new adress
	rei

	TRAPCALL(invkstk, T_KSPNOTVAL)
	STRAY(0, 0C)

	TRAPCALL(privinflt, T_PRIVINFLT)
	STRAY(0, 14)
	TRAPCALL(resopflt, T_RESOPFLT)
	TRAPCALL(resadflt, T_RESADFLT)

		.align	2
transl_v:	.globl	transl_v	# Translation violation, 20
	pushl	$T_TRANSFLT
L3:	bbc	$1,4(sp),L1
	bisl2	$T_PTEFETCH, (sp)
L1:	bbc	$2,4(sp),L2
	bisl2	$T_WRITE, (sp)
L2:	movl	(sp), 4(sp)
	addl2	$4, sp
	jbr	trap


		.align  2
access_v:.globl	access_v	# Access cntrl viol fault, 	24
	blbs	(sp), ptelen
	pushl	$T_ACCFLT
	jbr	L3

ptelen:	movl	$T_PTELEN, (sp)		# PTE must expand (or send segv)
	jbr trap;

	TRAPCALL(tracep, T_TRCTRAP)
	TRAPCALL(breakp, T_BPTFLT)
	STRAY(0, 30)

	TRAPARGC(arithflt, T_ARITHFLT)

	STRAY(0, 38)
	STRAY(0, 3C)





	.align 2		# Main system call 
	.globl	syscall
syscall:
	pushl	$T_SYSCALL
	pushr	$0xfff
	pushl	ap
	pushl	fp
	pushl	sp		# pointer to syscall frame; defined in trap.h
	calls	$1,_syscall
	movl	(sp)+,fp
	movl	(sp)+,ap
	popr	$0xfff
	addl2	$8,sp
	mtpr	$0x1f,$PR_IPL	# Be sure we can REI
	rei

	STRAY(0, 44)
	STRAY(0, 48)
	STRAY(0, 4C)
	STRAY(0, 50)
	FASTINTR(cmrerr, cmrerr)
	STRAY(0, 58)
	STRAY(0, 5C)
	STRAY(0, 60)
	STRAY(0, 64)
	STRAY(0, 68)
	STRAY(0, 6C)
	STRAY(0, 70)
	STRAY(0, 74)
	STRAY(0, 78)
	STRAY(0, 7C)
	STRAY(0, 80)
	STRAY(0, 84)

	TRAPCALL(astintr, T_ASTFLT)

	STRAY(0, 8C)
	STRAY(0, 90)
	STRAY(0, 94)
	STRAY(0, 98)
	STRAY(0, 9C)

	FASTINTR(softclock, softclock)

	STRAY(0, A4)
	STRAY(0, A8)
	STRAY(0, AC)

	FASTINTR(netint, netintr)	#network packet interrupt

	STRAY(0, B4)
	STRAY(0, B8)
	TRAPCALL(ddbtrap,T_KDBTRAP)

		.align	2
		.globl	hardclock
hardclock:	mtpr	$0xc1,$PR_ICCS		# Reset interrupt flag
		pushr	$0x3f
		pushl	sp
		addl2	$24,(sp)
		calls	$1,_hardclock
		popr	$0x3f
		rei

	STRAY(0, C4)
	STRAY(0, CC)
	STRAY(0, D0)
	STRAY(0, D4)
	STRAY(0, D8)
	STRAY(0, DC)
	STRAY(0, E0)
	STRAY(0, E4)
	STRAY(0, E8)
	STRAY(0, EC)

#ifdef VAX750
	FASTINTR(cstrint, cturintr)
	FASTINTR(csttint, ctutintr)
#else
	STRAY(0, F0)
	STRAY(0, F4)
#endif

	FASTINTR(consrint, gencnrint)
	FASTINTR(constint, gencntint)

trap:	pushr	$0xfff
	pushl	ap
	pushl	fp
	pushl	sp
	calls	$1,_arithflt
	movl	(sp)+,fp
	movl	(sp)+,ap
        popr	$0xfff
	addl2	$8,sp
	mtpr	$0x1f,$PR_IPL	# Be sure we can REI
	rei

#if VAX630 || VAX650
/*
 * Table of emulated Microvax instructions supported by emulate.s.
 * Use noemulate to convert unimplemented ones to reserved instruction faults.
 */
	.globl	_emtable
_emtable:
/* f8 */ .long _EMashp; .long _EMcvtlp; .long noemulate; .long noemulate
/* fc */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 00 */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 04 */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 08 */ .long _EMcvtps; .long _EMcvtsp; .long noemulate; .long _EMcrc
/* 0c */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 10 */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 14 */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 18 */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 1c */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 20 */ .long _EMaddp4; .long _EMaddp6; .long _EMsubp4; .long _EMsubp6
/* 24 */ .long _EMcvtpt; .long _EMmulp; .long _EMcvttp; .long _EMdivp
/* 28 */ .long noemulate; .long _EMcmpc3; .long _EMscanc; .long _EMspanc
/* 2c */ .long noemulate; .long _EMcmpc5; .long _EMmovtc; .long _EMmovtuc
/* 30 */ .long noemulate; .long noemulate; .long noemulate; .long noemulate
/* 34 */ .long _EMmovp; .long _EMcmpp3; .long _EMcvtpl; .long _EMcmpp4
/* 38 */ .long _EMeditpc; .long _EMmatchc; .long _EMlocc; .long _EMskpc
#endif
/*
 * The following is called with the stack set up as follows:
 *
 *	  (sp):	Opcode
 *	 4(sp):	Instruction PC
 *	 8(sp):	Operand 1
 *	12(sp):	Operand 2
 *	16(sp):	Operand 3
 *	20(sp):	Operand 4
 *	24(sp):	Operand 5
 *	28(sp):	Operand 6
 *	32(sp):	Operand 7 (unused)
 *	36(sp):	Operand 8 (unused)
 *	40(sp):	Return PC
 *	44(sp):	Return PSL
 *	48(sp): TOS before instruction
 *
 * Each individual routine is called with the stack set up as follows:
 *
 *	  (sp):	Return address of trap handler
 *	 4(sp):	Opcode (will get return PSL)
 *	 8(sp):	Instruction PC
 *	12(sp):	Operand 1
 *	16(sp):	Operand 2
 *	20(sp):	Operand 3
 *	24(sp):	Operand 4
 *	28(sp):	Operand 5
 *	32(sp):	Operand 6
 *	36(sp):	saved register 11
 *	40(sp):	saved register 10
 *	44(sp):	Return PC
 *	48(sp):	Return PSL
 *	52(sp): TOS before instruction
 *	See the VAX Architecture Reference Manual, Section B-5 for more
 *	information.
 */

	.align	2
	.globl	emulate
emulate:
#if VAX630 || VAX650
	movl	r11,32(sp)		# save register r11 in unused operand
	movl	r10,36(sp)		# save register r10 in unused operand
	cvtbl	(sp),r10		# get opcode
	addl2	$8,r10			# shift negative opcodes
	subl3	r10,$0x43,r11		# forget it if opcode is out of range
	bcs	noemulate
	movl	_emtable[r10],r10	# call appropriate emulation routine
	jsb	(r10)		# routines put return values into regs 0-5
	movl	32(sp),r11		# restore register r11
	movl	36(sp),r10		# restore register r10
	insv	(sp),$0,$4,44(sp)	# and condition codes in Opcode spot
	addl2	$40,sp			# adjust stack for return
	rei
noemulate:
	addl2	$48,sp			# adjust stack for
#endif
	.word	0xffff			# "reserved instruction fault"

        .globl  _intrnames, _eintrnames, _intrcnt, _eintrcnt
_intrnames:
        .long   0
_eintrnames:
_intrcnt:
        .long   0
_eintrcnt:

	.data
_scb:	.long 0
	.globl _scb

