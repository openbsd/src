/*	$OpenBSD: locore.S,v 1.1 2010/05/26 16:35:28 deraadt Exp $   */
/*	$NetBSD: intvec.s,v 1.39 1999/06/28 08:20:48 itojun Exp $   */

/*
 * Copyright (c) 1994, 1997 Ludd, University of Lule}, Sweden.
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


#include "assym.h"

#include <net/netisr.h>

#define ENTRY(name) \
	.text			; \
	.align 2		; \
	.globl name		; \
name /**/:

#define TRAPCALL(namn, typ) \
ENTRY(namn)			; \
	pushl $0		; \
	pushl $typ		; \
	jbr trap

#define TRAPARGC(namn, typ) \
ENTRY(namn)			; \
	pushl $typ		; \
	jbr trap

#define FASTINTR(namn, rutin) \
ENTRY(namn)			; \
	pushr $0x3f		; \
	calls $0,_/**/rutin	; \
	popr $0x3f		; \
	rei

#define	PUSHR	pushr	$0x3f
#define	POPR	popr	$0x3f

#define KSTACK 0
#define ISTACK 1
#define	NOVEC	.long 0
#define INTVEC(label,stack)	\
	.long	label+stack;
		.text

	.globl	_kernbase, _rpb, _kernel_text
	.set	_kernel_text,KERNBASE
_kernbase:
_rpb:	
/*
 * First page in memory we have rpb; so that we know where
 * (must be on a 64k page boundary, easiest here). We use it
 * to store SCB vectors generated when compiling the kernel,
 * and move the SCB later to somewhere else.
 */

	NOVEC;				# Unused, 0
	INTVEC(mcheck, ISTACK)		# Machine Check., 4
	INTVEC(invkstk, ISTACK) 	# Kernel Stack Invalid., 8
	NOVEC;			 	# Power Failed., C
	INTVEC(privinflt, KSTACK)	# Privileged/Reserved Instruction.
	INTVEC(xfcflt, KSTACK)		# Customer Reserved Instruction, 14
	INTVEC(resopflt, KSTACK)	# Reserved Operand/Boot Vector(?), 18
	INTVEC(resadflt, KSTACK)	# Reserved Address Mode., 1C
	INTVEC(access_v, KSTACK)	# Access Control Violation, 20
	INTVEC(transl_v, KSTACK)	# Translation Invalid, 24
	INTVEC(tracep, KSTACK)		# Trace Pending, 28
	INTVEC(breakp, KSTACK)		# Breakpoint Instruction, 2C
	NOVEC;			 	# Compatibility Exception, 30
	INTVEC(arithflt, KSTACK)	# Arithmetic Fault, 34
	NOVEC;			 	# Unused, 38
	NOVEC;			 	# Unused, 3C
	INTVEC(syscall, KSTACK)		# main syscall trap, chmk, 40
	INTVEC(resopflt, KSTACK)	# chme, 44
	INTVEC(resopflt, KSTACK)	# chms, 48
	INTVEC(resopflt, KSTACK)	# chmu, 4C
	NOVEC;				# System Backplane Exception/BIerror, 50
	INTVEC(cmrerr, ISTACK)		# Corrected Memory Read, 54
	NOVEC;				# System Backplane Alert/RXCD, 58
	INTVEC(sbiflt, ISTACK)		# System Backplane Fault, 5C
	NOVEC;				# Memory Write Timeout, 60
	NOVEC;				# Unused, 64
	NOVEC;				# Unused, 68
	NOVEC;				# Unused, 6C
	NOVEC;				# Unused, 70
	NOVEC;				# Unused, 74
	NOVEC;				# Unused, 78
	NOVEC;				# Unused, 7C
	NOVEC;				# Unused, 80
	NOVEC;				# Unused, 84
	INTVEC(astintr,	KSTACK)		# Asynchronous System Trap, AST
	NOVEC;				# Unused, 8C
	NOVEC;				# Unused, 90
	NOVEC;				# Unused, 94
	NOVEC;				# Unused, 98
	NOVEC;				# Unused, 9C
	INTVEC(softintr,ISTACK)		# Software interrupts (IPL_SOFT)
	INTVEC(softintr,ISTACK)		# Software interrupts (IPL_SOFTCLOCK)
	INTVEC(softintr,ISTACK)		# Software interrupts (IPL_SOFTNET)
	INTVEC(softintr,ISTACK)		# Software interrupts (IPL_SOFTTTY)
	NOVEC;				# Unused, B0
	NOVEC;				# Unused, B4
	NOVEC;				# Unused, B8
	INTVEC(ddbtrap, ISTACK) 	# Kernel debugger trap, BC
	INTVEC(hardclock,ISTACK)	# Interval Timer
	NOVEC;				# Unused, C4
	INTVEC(emulate, KSTACK)		# Subset instruction emulation, C8
	NOVEC;				# Unused, CC
	NOVEC;				# Unused, D0
	NOVEC;				# Unused, D4
	NOVEC;				# Unused, D8
	NOVEC;				# Unused, DC
	NOVEC;				# Unused, E0
	NOVEC;				# Unused, E4
	NOVEC;				# Unused, E8
	NOVEC;				# Unused, EC
	NOVEC;		
	NOVEC;		
	NOVEC;		
	NOVEC;		

	/* space for adapter vectors */
	.space 0x100

		.align 2
#
# mcheck is the badaddress trap, also called when referencing
# a invalid address (busserror)
# _memtest (memtest in C) holds the address to continue execution
# at when returning from a intentional test.
#
mcheck: .globl	mcheck
	tstl	_cold		# Are we still in coldstart?
	bneq	L4		# Yes.

	pushr	$0x7f
	pushab	24(sp)
	movl	_dep_call,r6	# CPU dependent mchk handling
	calls	$1,*MCHK(r6)
	tstl	r0		# If not machine check, try memory error
	beql	1f
	calls	$0,*MEMERR(r6)
	pushab	2f
	calls	$1,_panic
2:	.asciz	"mchk"
1:	popr	$0x7f
	addl2	(sp)+,sp

	rei

L4:	addl2	(sp)+,sp	# remove info pushed on stack
	cmpl	_vax_cputype,$1 # Is it a 11/780?
	bneq	1f		# No...

	mtpr	$0, $PR_SBIFS	# Clear SBI fault register
	brb	2f

1:	cmpl	_vax_cputype,$4 # Is it a 8600?
	bneq	3f

	mtpr	$0, $PR_EHSR	# Clear Error status register
	brb	2f

3:	mtpr	$0xF,$PR_MCESR	# clear the bus error bit
2:	movl	_memtest,(sp)	# REI to new address
	rei

TRAPCALL(invkstk, T_KSPNOTVAL)

ENTRY(privinflt)	# Privileged/unimplemented instruction
#ifdef INSN_EMULATE
	jsb	unimemu	# do not return if insn emulated
#endif
	pushl $0
	pushl $T_PRIVINFLT
	jbr trap

TRAPCALL(xfcflt, T_XFCFLT);
TRAPCALL(resopflt, T_RESOPFLT)
TRAPCALL(resadflt, T_RESADFLT)

/*
 * Translation fault, used only when simulating page reference bit.
 * Therefore it is done a fast revalidation of the page if it is
 * referenced. Trouble here is the hardware bug on KA650 CPUs that
 * put in a need for an extra check when the fault is gotten during
 * PTE reference. Handled in pmap.c.
 */
	.align	2
	.globl	transl_v	# 20: Translation violation
transl_v:
	PUSHR
	pushl	28(sp)
	pushl	28(sp)
	calls	$2,_pmap_simulref
	tstl	r0
	bneq	1f
	POPR
	addl2	$8,sp
	rei
1:	POPR
	brb	access_v

	.align	2
	.globl	access_v	# 24: Access cntrl viol fault
access_v:
	blbs	(sp), ptelen
	pushl	$T_ACCFLT
	bbc	$1,4(sp),1f
	bisl2	$T_PTEFETCH,(sp)
1:	bbc	$2,4(sp),2f
	bisl2	$T_WRITE,(sp)
2:	movl	(sp), 4(sp)
	addl2	$4, sp
	jbr	trap

ptelen: movl	$T_PTELEN, (sp)		# PTE must expand (or send segv)
	jbr trap;

TRAPCALL(tracep, T_TRCTRAP)
TRAPCALL(breakp, T_BPTFLT)

TRAPARGC(arithflt, T_ARITHFLT)

ENTRY(syscall)			# Main system call
	pushl	$T_SYSCALL
	pushr	$0xfff
	mfpr	$PR_USP, -(sp)
	pushl	ap
	pushl	fp
	pushl	sp		# pointer to syscall frame; defined in trap.h
	calls	$1, _syscall
	movl	(sp)+, fp
	movl	(sp)+, ap
	mtpr	(sp)+, $PR_USP
	popr	$0xfff
	addl2	$8, sp
	mtpr	$0x1f, $PR_IPL	# Be sure we can REI
	rei


ENTRY(cmrerr)
	PUSHR
	movl	_dep_call,r0
	calls	$0,*MEMERR(r0)
	POPR
	rei

ENTRY(sbiflt)
	pushab	sbifltmsg
	calls	$1, _panic

TRAPCALL(astintr, T_ASTFLT)

FASTINTR(softintr,softintr_dispatch)

	.data
	.global _netisr
_netisr:
	.long	0	# scheduling bits for network

TRAPCALL(ddbtrap, T_KDBTRAP)

ENTRY(hardclock)
	mtpr	$0xc1,$PR_ICCS		# Reset interrupt flag
	PUSHR
	pushl	sp
	addl2	$24,(sp)
	movl	_dep_call,r0
	calls	$1,*HARDCLOCK(r0)
	incl	_clock_intrcnt+EC_COUNT		# increment low longword
	adwc	$0,_clock_intrcnt+EC_COUNT+4	# add any carry to hi longword
	POPR
	rei

/*
 * Main routine for traps; all go through this.
 * Note that we put USP on the frame here, which sometimes should
 * be KSP to be correct, but because we only alters it when we are 
 * called from user space it doesn't care.
 * _sret is used in cpu_set_kpc to jump out to user space first time.
 */
	.globl	_sret
trap:	pushr	$0xfff
	mfpr	$PR_USP, -(sp)
	pushl	ap
	pushl	fp
	pushl	sp
	calls	$1, _arithflt
_sret:	movl	(sp)+, fp
	movl	(sp)+, ap
	mtpr	(sp)+, $PR_USP
	popr	$0xfff
	addl2	$8, sp
	mtpr	$0x1f, $PR_IPL	# Be sure we can REI
	rei

sbifltmsg:
	.asciz	"SBI fault"

#if INSN_EMULATE
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
 *	  (sp): Opcode
 *	 4(sp): Instruction PC
 *	 8(sp): Operand 1
 *	12(sp): Operand 2
 *	16(sp): Operand 3
 *	20(sp): Operand 4
 *	24(sp): Operand 5
 *	28(sp): Operand 6
 *	32(sp): Operand 7 (unused)
 *	36(sp): Operand 8 (unused)
 *	40(sp): Return PC
 *	44(sp): Return PSL
 *	48(sp): TOS before instruction
 *
 * Each individual routine is called with the stack set up as follows:
 *
 *	  (sp): Return address of trap handler
 *	 4(sp): Opcode (will get return PSL)
 *	 8(sp): Instruction PC
 *	12(sp): Operand 1
 *	16(sp): Operand 2
 *	20(sp): Operand 3
 *	24(sp): Operand 4
 *	28(sp): Operand 5
 *	32(sp): Operand 6
 *	36(sp): saved register 11
 *	40(sp): saved register 10
 *	44(sp): Return PC
 *	48(sp): Return PSL
 *	52(sp): TOS before instruction
 *	See the VAX Architecture Reference Manual, Section B-5 for more
 *	information.
 */

	.align	2
	.globl	emulate
emulate:
#if INSN_EMULATE
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

	.data
_scb:	.long 0
	.globl _scb

