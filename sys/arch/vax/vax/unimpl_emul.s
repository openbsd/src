/*	$OpenBSD: unimpl_emul.s,v 1.2 2000/10/24 02:20:21 hugh Exp $	*/
/*	$NetBSD: unimpl_emul.s,v 1.2 2000/08/14 11:16:52 ragge Exp $	*/

/*
 * Copyright (c) 2000 Ludd, University of Lule}, Sweden. All rights reserved.
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
 *      This product includes software developed at Ludd, University of
 *      Lule}, Sweden and its contributors.
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


#include <machine/asm.h>
#include "assym.h"

# Only intended for debugging emulation code (security hole)
#undef	EMULATE_INKERNEL

# Defines to fetch register operands
#define	S_R0	(fp)
#define	S_R1	4(fp)
#define	S_R2	8(fp)
#define	S_R3	12(fp)
#define	S_R4	16(fp)
#define	S_R5	20(fp)
#define	S_R6	24(fp)
#define	S_R7	28(fp)
#define	S_R8	32(fp)
#define	S_R9	36(fp)
#define	S_R10	40(fp)
#define	S_R11	44(fp)
#define	S_AP	48(fp)
#define	S_FP	52(fp)
#define	S_SP	56(fp)
#define	S_PC	60(fp)
#define	S_PSL	64(fp)

#
# Emulation of instruction trapped via SCB vector 0x18. (reserved op)
#
.globl unimemu; unimemu:
	pushl	r0
	movl	8(sp),r0	# get trap address
	movzbl	(r0),r0		# fetch insn generating trap
	caseb	r0,$0x75,$0	# case to jump to address
0:	.word	polyd-0b

1:	movl	(sp)+,r0	# restore reg
	rsb			# continue fault

#
# switch the code back over to user mode.
# puts the psl + pc (+ jsb return address) on top of user stack.
#
#ifdef EMULATE_INKERNEL
touser:	movl	(sp),-52(sp)	# save rsb address on top of new stack
	movl	4(sp),r0	# restore saved reg
	addl2	$12,sp		# pop junk from stack
	pushr	$0x7fff		# save all regs
	movl	sp,fp		# new frame pointer
	tstl	-(sp)		# remember old rsb address
	incl	S_PC		# skip matching insn
	rsb
#else
touser:	mfpr	$PR_USP,r0	# get user stack pointer
	movl	4(sp),-68(r0)	# move already saved r0
	movl	(sp),-72(r0)	# move return address
	movq	12(sp),-8(r0)	# move pc + psl
	addl2	$12,sp		# remove moved fields from stack
	movl	$1f,(sp)	# change return address
	rei
1:	subl2	$8,sp		# trapaddr + psl already on stack
	pushr	$0x7ffe		# r0 already saved
	subl2	$8,sp		# do not trash r0 + retaddr
	movab	4(sp),fp
	incl	S_PC		# skip matching insn
	rsb
#endif

#
# Restore registers, cleanup and continue
#
goback:	movl	fp,sp		# be sure
	popr	$0x7fff		# restore all regs
	rei
#
# getval_dfloat get 8 bytes and stores them in r0/r1. Increases PC.
#
getval_dfloat:
	clrq	r0
	pushr	$(R2+R3)	# use r2+r3 as scratch reg
	movl	S_PC,r3		# argument address
	extzv	$4,$4,(r3),r2	# get mode
	caseb	r2,$0,$5
0:	.word	1f-0b		# 0-3 literal
	.word	1f-0b
	.word	1f-0b
	.word	1f-0b
	.word	2f-0b		# 4 indexed
	.word	3f-0b		# 5 register
#ifdef EMULATE_INKERNEL
2:	movab	0f,r0
	movl	r2,r1
	brw	die
0:	.asciz	"getval_dfloat: missing address mode %d\n"
#else
2:	.word 	0xffff		# reserved operand
#endif

1:	insv	(r3),$0,$3,r0	# insert fraction
	extzv	$3,$3,(r3),r2	# get exponent
	addl2	$128,r2		# bias the exponent
	insv	r2,$7,$8,r0	# insert exponent
	tstl	(r3)+
	brb 4f

3:	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	ashl	$2,r2,r2
	addl2	fp,r2
	movq	(r2),r0

4:	movl	r3,S_PC
	popr	$(R2+R3)
	rsb

#
# getval_word get 2 bytes and stores them zero-extended in r0. Increases PC.
#
getval_word:
	clrl	r0
	pushr	$(R2+R3)	# use r2+r3 as scratch reg
	movl	S_PC,r3		# argument address
	extzv	$4,$4,(r3),r2	# get mode
	caseb	r2,$0,$5
0:	.word	1f-0b		# 0-3 literal
	.word	1f-0b
	.word	1f-0b
	.word	1f-0b
	.word	2f-0b		# 4 indexed
	.word	3f-0b		# 5 register
#ifdef EMULATE_INKERNEL
2:	movab	0f,r0
	movl	r2,r1
	brw	die
0:	.asciz	"getval_word: missing address mode %d\n"
#else
2:	.word 	0xffff		# reserved operand
#endif

1:	movb	(r3)+,r0	# correct operand
	brb 4f

3:	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	ashl	$2,r2,r2
	addl2	fp,r2
	movw	(r2),r0

4:	movl	r3,S_PC
	popr	$(R2+R3)
	rsb

#
# getaddr_byte get 4 bytes and stores them in r0. Increases PC.
#
getaddr_byte:
	clrl	r0
	pushr	$(R2+R3)	# use r2+r3 as scratch reg
	movl	S_PC,r3		# argument address
	extzv	$4,$4,(r3),r2	# get mode
	caseb	r2,$6,$9
0:	.word	5f-0b		# 6 deferred
	.word	2f-0b		# 7 autodecr (missing)
	.word	2f-0b		# 8 autoincr (missing)
	.word	2f-0b		# 9 autoincr deferred (missing)
	.word	2f-0b		# 10 byte disp (missing)
	.word	2f-0b		# 11 byte disp deferred (missing)
	.word	2f-0b		# 12 word disp (missing)
	.word	2f-0b		# 13 word disp deferred (missing)
	.word	1f-0b		# 14 long disp
	.word	2f-0b		# 15 long disp deferred (missing)
#ifdef EMULATE_INKERNEL
2:	movab	3f,r0
	movl	r2,r1
	brw	die		# reserved operand
3:	.asciz	"getaddr_byte: missing address mode %d\n"
#else
2:	.word	0xffff		# reserved operand
#endif

1:	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	movl	(fp)[r2],r0	# Register contents
	addl2	(r3),r0		# add displacement
	cmpl	r2,$15		# pc?
	bneq	0f		# no, skip
	addl2	$5,r0		# compensate for displacement size
0:	addl2	$4,r3		# increase pc
	brb	4f

5:	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	movl	(fp)[r2],r0

4:	movl	r3,S_PC
	popr	$(R2+R3)
	rsb

#
# Polynomial calculation, d-float
# Uses d-float instructions, so hopefully d-float is available.
#
# polyd MISSING:
#	- check for bad arguments
#	- set PSL flags
#	- do not use d-float instructions (may be emulated)
#
polyd:	bsbw	touser		# go back to user mode
	bsbw	getval_dfloat	# fetches argument to r0/r1
	movq	r0,r6
	bsbw	getval_word
	movl	r0,r4
	bsbw	getaddr_byte
	movl	r0,r3
	clrq	r0
# Ok, do the real calculation (Horner's method)
0:	addd2	(r3)+,r0	# add constant
	tstl	r4		# more?
	beql	1f		# no, exit
	muld2	r6,r0		# multiply with arg
	decl	r4		# lower degree
	brb	0b

1:	movq	r0,(fp)
	clrl	S_R2
	movl	r3,S_R3
	clrq	S_R4
	brw	goback


#ifdef EMULATE_INKERNEL
# When we ends up somewhere we don't want.
die:	pushl	r1
	pushl	r0
	calls	$2,_printf
	movl	fp,sp
	brw	goback		# anything may happen
#endif
