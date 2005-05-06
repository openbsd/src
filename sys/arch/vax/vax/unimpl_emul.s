/*	$OpenBSD: unimpl_emul.s,v 1.8 2005/05/06 18:55:02 miod Exp $	*/
/*	$NetBSD: unimpl_emul.s,v 1.2 2000/08/14 11:16:52 ragge Exp $	*/

/*
 * Copyright (c) 2001 Brandon Creighton.  All rights reserved.  
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

#include "assym.h"

#include <machine/asm.h>
#include <machine/psl.h>

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

#define PSL_Q  	(PSL_N | PSL_Z | PSL_V | PSL_C)
				
#
# Emulation of instruction trapped via SCB vector 0x18. (reserved op)
#
.globl unimemu; unimemu:
	pushl	r0
	movl	8(sp),r0	# get trap address
	movzbl	(r0),r0		# fetch insn generating trap
	caseb	r0,$0x74,$1	# case to jump to address
0:	.word	emodd-0b
	.word	polyd-0b

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
	
/*
 * getval: is used by the getval_* functions.  Gets the value specified by the 
 * current operand specifier pointed to by S_PC.  It also increments S_PC.
 */
getval:
	clrq	r0
	pushr	$(R2+R3+R4+R5+R6)
	movl	S_PC,r3		# argument address
	extzv	$4,$4,(r3),r2	# get mode
	caseb	r2,$0,$0xf
0:	.word	getval_literal-0b		# 0-3 literal
	.word	getval_literal-0b
	.word	getval_literal-0b
	.word	getval_literal-0b
	.word	2f-0b		# 4 indexed
	.word	getval_reg-0b			# 5 register
	.word	getval_regdefer-0b		# 6 register deferred
	.word	2f-0b		# 7 register deferred
	.word	getval_ai-0b			# 8 autoincrement
	.word	2f-0b		# 9 autoincrement deferred 
	.word	getval_bytedis-0b		# A byte displacement 
	.word	2f-0b		# B byte displacement deferred
	.word	2f-0b		# C word displacement
	.word	2f-0b		# D word displacement deferred
	.word	getval_longdis-0b		# E longword displacement
	.word	2f-0b		# F longword displacement deferred
#ifdef EMULATE_INKERNEL
2:	movab	0f,r0
	movl	r2,r1
	brw	die
0:	.asciz	"getval: missing address mode %d\n"
#else
2:	.word 	0xffff		# reserved operand
#endif

	/*
	 * 0x00-0x03  
	 * Literal mode.  Note:  getval_{d,f}float will *never* use this routine
	 * to get literal values, since they treat them differently (see those routines
	 * for details).
	 */
getval_literal:
	movzbl	(r3)+,r0	# correct operand
	brw 4f

	/* 
	 * 0x05  
     * Register mode.  Grab the register number, yank the value out.
	 */
getval_reg:	
	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	ashl	$2,r2,r2
	addl3	fp,r2,r5
	bsbw	emul_extract
	brw		4f

	/* 
	 * 0x06
     * Register deferred mode.  Grab the register number, yank the value out,
	 * use that as the address to get the real value.
	 */
getval_regdefer:	
	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	ashl	$2,r2,r2
	addl2	fp,r2
	movl	(r2),r5
	bsbw	emul_extract
	brw		4f

	/* 
	 * 0x08 Autoincrement mode
     * Get the value in the register, use that as the address of our target,
	 * then increment the register.
	 */
getval_ai:
	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3

	/*
	 * In the case of the register being PC (0xf), this is called immediate mode;
	 * we can treat it the same as any other register, as long as we keep r3
     * and S_PC in sync.  We do that here.	 
	 */
	movl 	r3,S_PC

	ashl	$2,r2,r2
	addl2	fp,r2
	movl	(r2),r5
	bsbw	emul_extract
	addl2	r6,(r2)

	movl	S_PC,r3		/* if PC did change, S_PC was changed too */
	brw		4f

	/*
	 * 0xA
	 * Byte displacement mode.
     */	 
getval_bytedis: 
	extzv	$0, $4, (r3), r2	# get register 
	incl	r3
	ashl	$2,r2,r2
	addl2	fp,r2
	movl	(r2),r5
	movzbl	(r3),r4
	incl	r3
	addl2	r4, r5
	bsbw	emul_extract
	brw		4f

	/*
	 * 0xE  
	 * Longword displacement mode.
     */	 
getval_longdis: 
	extzv	$0, $4, (r3), r2	# get register 
	incl	r3
	ashl	$2,r2,r2
	addl2	fp,r2
	movl	(r2),r5
	movl	(r3)+,r4
	addl2	r4, r5
	bsbw	emul_extract

4:	movl	r3,S_PC
	popr	$(R2+R3+R4+R5+R6)
	rsb

/*
 * emul_extract: used by the getval functions.  This extracts exactly r6 bytes 
 * from the address in r5 and places them in r0 and r1 (if necessary).
 * 8 is the current maximum length.
 */
emul_extract:
	cmpl $0x8, r6
	bgeq 1f
	.word 	0xffff		# reserved operand
1:	
	caseb r6, $0x1, $0x7
0:	.word 1f-0b			# 1: byte
	.word 2f-0b			# 2: word
	.word 9f-0b			# unknown
	.word 4f-0b			# 4: longword
	.word 9f-0b			# unknown
	.word 9f-0b			# unknown
	.word 9f-0b			# unknown
	.word 8f-0b			# 8: quadword

1:	movzbl (r5), r0
	rsb

2:	movzwl (r5), r0
	rsb

4:	movl (r5), r0
	rsb

8:	movq (r5), r0
	rsb

9:	
	.word 	0xffff		# reserved operand
	rsb

getval_dfloat:
	clrq	r0
	pushr	$(R2+R3+R6)	# use r2+r3 as scratch reg
	movl	S_PC,r3		# argument address 
	extzv	$4,$4,(r3),r2	# get mode
	caseb	r2,$0,$0x3
0:	.word	1f-0b		# 0-3 literal
	.word	1f-0b
	.word	1f-0b
	.word	1f-0b

	movl	$0x8, r6
	bsbw	getval
	brw		4f

1:	insv	(r3),$0,$3,r0	# insert fraction
	extzv	$3,$3,(r3),r2	# get exponent
	addl2	$128,r2		# bias the exponent
	insv	r2,$7,$8,r0	# insert exponent
	tstb	(r3)+
	movl	r3,S_PC
4:
	popr	$(R2+R3+R6)
	rsb

getval_long:
	clrl	r0
	pushr	$(R6+R1)	
	movl	$0x4, r6
	bsbw	getval
	popr	$(R6+R1)
	rsb

getval_word:
	clrl	r0
	pushr	$(R6+R1)
	movl	$0x2, r6
	bsbw	getval
	popr	$(R6+R1)
	rsb

getval_byte:
	clrl	r0
	pushr	$(R6+R1)	# use r2+r3 as scratch reg
	movl	$0x1, r6
	bsbw	getval
	popr	$(R6+R1)
	rsb

#
# getaddr_byte get 4 bytes and stores them in r0. Increases PC.
#
getaddr_byte:
	clrl	r0
	pushr	$(R2+R3)	# use r2+r3 as scratch reg
	movl	S_PC,r3		# argument address
	extzv	$4,$4,(r3),r2	# get mode
	caseb	r2,$0,$0xf
0:	.word	2f-0b		# 0-3 literal	
	.word	2f-0b
	.word	2f-0b
	.word	2f-0b
	.word	2f-0b		# 4 
	.word	6f-0b		# 5 register
	.word	5f-0b		# 6 deferred
	.word	2f-0b		# 7 autodecr (missing)
	.word	2f-0b		# 8 autoincr (missing)
	.word	2f-0b		# 9 autoincr deferred (missing)
	.word	7f-0b		# 10 byte disp 
	.word	2f-0b		# 11 byte disp deferred (missing)
	.word	8f-0b		# 12 word disp
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
	brw	4f

5:	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	movl	(fp)[r2],r0
	brw		4f

7:	
	extzv	$0, $4, (r3), r2	# get register
	incl	r3
	movl	(fp)[r2],r0	# Register contents
	pushl	r4
	cvtbl	(r3),r4
	addl2	r4,r0		# add displacement
	movl	(sp)+,r4
	cmpl	r2,$15		# pc?
	bneq	0f		# no, skip
	addl2	$2,r0		# compensate for displacement size
0:	incl	r3		# increase pc
	brw	4f

8:
	extzv	$0, $4, (r3), r2	# get register
	incl	r3
	movl	(fp)[r2],r0	# Register contents
	pushl	r4
	cvtwl	(r3),r4
	addl2	r4,r0		# add displacement
	movl	(sp)+,r4
	cmpl	r2,$15		# pc?
	bneq	0f		# no, skip
	addl2	$3,r0		# compensate for displacement size
0:	addl2	$2,r3		# increase pc
	brw	4f

6:	extzv	$0,$4,(r3),r2	# Get reg number
	incl	r3
	moval	(fp)[r2],r0

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
# When we end up somewhere we don't want.
die:	pushl	r1
	pushl	r0
	calls	$2,_printf
	movl	fp,sp
	brw	goback		# anything may happen
#endif

# these emodd-related
#define TMPSIZE 0x20	/* temp bytes -- be careful with this! */
#define PRECIS 0x7
#define TMPFRAC1 (ap)
#define TMPFRAC2 32(ap)
#define TMPFRACTGT 64(ap)
#	
# Extended multiply/modulus	
# XXX just EMODD for now
emodd:	bsbw	touser

	/* Clear the condition codes; we will set them as needed later. */
	bicl2 $PSL_Q, S_PSL

	/* 
	 * We temporarily appropriate ap for the use of TMPFRAC*.
	 */
	pushl ap	
	subl2 $(3*TMPSIZE), sp
	movl sp, ap

	movc5 $0x0, TMPFRAC1, $0x0, $TMPSIZE, TMPFRAC1 
	movc5 $0x0, TMPFRAC2, $0x0, $TMPSIZE, TMPFRAC2 
	movc5 $0x0, TMPFRACTGT, $0x0, $TMPSIZE, TMPFRACTGT 

	clrl -(sp)
	movl sp, r3		/* r3 = addr of exp space (1) */
	clrl -(sp)
	movl sp, r5		/* r5 = addr of exp space (2) */
	subl2 $0x10, sp		
	movl sp, r6		/* r6 = addr of allocated target space */

	/*
	 * Now we package both numbers up and call fltext_De, which 
	 * will remove the exponent and sign; this will make them
	 * easier to work with.  They will be in TMPFRAC1 and 
	 * TMPFRAC2 when done.
	 */
	bsbw getval_dfloat 	# get operand into r0 and r1

	/* Check for sign = 0 and exp = 0; if it is, zeroexit. */
	bicl3 $0x7f, r0, r4
	cmpl r4, $0x0
	bneq 1f
	bsbw getval_byte	# get multiplier extension operand
	bsbw getval_dfloat	# get target operand
	jmp zeroexit
1:

	/* Check for sign = 1 and exp = 0; if it is, do a resopflt. */
	cmpw r0, $0x8000
	bneq 1f
	bsbw getval_byte	# get multiplier extension operand
	bsbw getval_dfloat 	# get operand into r0 and r1
	extzv $0, $0xff, r0, r0		# generate a resopflt -- XXX is this ok?
1:	
	movd r0, TMPFRACTGT
	bicl3 $0xffff7fff, r0, r6 # Extract the sign while we're here.
	bsbw getval_byte	# get multiplier extension operand
	movzbl r0, -(sp)
	movd r9, r0
	pushl r3
	pushab TMPFRAC1
	movab TMPFRACTGT, -(sp)
	calls $0x4, fltext_De

	bsbw getval_dfloat 	# get operand into r0 and r1

	/* Check for sign = 0 and exp = 0; if it is, zeroexit. */
	bicl3 $0x7f, r0, r4
	cmpl r4, $0x0
	bneq 1f
	bsbw getval_byte	# get multiplier extension operand
	bsbw getval_dfloat	# get target operand
	jmp zeroexit
1:
	/* Check for sign = 1 and exp = 0; if it is, do a resopflt. */
	cmpw r0, $0x8000
	bneq 1f
	bsbw getval_byte	# get multiplier extension operand
	bsbw getval_dfloat 	# get operand into r0 and r1
	extzv $0, $0xff, r0, r0		# generate a resopflt -- XXX is this ok?
1:	

	movd r0, TMPFRACTGT
	bicl3 $0xffff7fff, r0, r7 # Extract the sign while we're here.
	movzbl $0x0, -(sp)	# no multiplier extension here
	pushl r5
	pushab TMPFRAC2
	movab TMPFRACTGT, -(sp)
	calls $0x4, fltext_De

	/* first, add exponents */
	addl3 (r5), (r3), r9	/* r9 = exponent (used later) */
	subl2 $0x80, r9			/* we are excess-128 */
	
	/*
	 * Let's calculate the target sign.  Signs from multipliers are in r6 and 
	 * r7, and both the fraction and integer parts have the same sign.
	 */
	xorl2 r7, r6

	pushab TMPFRAC1
	calls $0x1, bitcnt
	movl r0, r1			/* r1 = bitcount of TMPFRAC1 */
	pushab TMPFRAC2
	calls $0x1, bitcnt
	movl r0, r2			/* r2 = bitcount of TMPFRAC2 */

	/*
	 * Now we get ready to multiply.  This multiplies a byte at a time,
	 * converting to double with CVTLD and adding partial results to 
	 * TMPFRACTGT.  There's probably a faster way to do this.
	 */
	clrd TMPFRACTGT
	pushr $0x7fc
	subl2 $0x8, sp			/* make some temporary space */
	movl sp, r1
	subl2 $0x8, sp
	movl sp, r2

	movl $PRECIS, r5			/* r5 = TMPFRAC1 byte count */
	movl $PRECIS, r6			/* r6 = TMPFRAC2 byte count */
	clrl r7

1:
#	addl3 r5, $TMPFRAC1, r3		/* r3 - current byte in tmpfrac1 */
	movab TMPFRAC1, r7
	addl3 r5, r7, r3
#	addl3 r6, $TMPFRAC2, r4		/* r4 - current byte in tmpfrac2 */
	movab TMPFRAC2, r7
	addl3 r6, r7, r4

	movzbl (r3), r10
	movzbl (r4), r11
	mull3 r10, r11, r7
	movl r7, r3
	cvtld r7, (r2)

	subl3 r5, $0x8, r8
	subl3 r6, $0x8, r9
	addl2 r8, r9			
	mull2 $0x8, r9
	subl2 $0x40, r9
	blss 9f	

	/* This may be bigger than a longword.  Break it up. */
5:	cmpl r9, $0x1e
	bleq 6f
	subl2 $0x1e, r9

	ashl $0x1e, $0x1, r8
	cvtld r8, (r1) 
	muld2 (r1), (r2) 
	jmp 5b
6:
	ashl r9, $0x1, r8
	cvtld r8, (r1)
	muld2 (r1), (r2)
	addd2 (r2), TMPFRACTGT
	
9:
	cmpl r5, $0x0
	beql 2f
	decl r5
	jmp 1b
2:	cmpl r6, $0x0
	beql 3f
	decl r6
	movl $PRECIS, r5
	jmp 1b
3:

	/*
	 * At this point, r9 might not reflect the final exponent we will use;
	 * i.e., we need post-normalization.  Luckily, we still have (in r7) 
	 * the results from the last individual multiplication handy.  Here 
	 * we calculate how many bits it will take to shift the value in r7
	 * so that bit 15 = 1.
	 */
	addl2 $0x10, sp
	movl r7, 0x14(sp)	/* move r7 onto the frame we're about to pop off */
   	popr  $0x7fc

	clrl r3	/* r3 = counter */
	movl r7, r8		/* r8 = temp */
1:
	bicl3 $0xffff7fff, r8, r5
	bneq 2f
	incl r3
	ashl $0x1, r8, r5
	movl r5, r8
	jmp 1b
2:

	/* 
	 * Now we do post-normalization (by subtracting r3) and
	 * put the exponent (in r9) into TMPFRACTGT.
	 */
	subl2 r3, r9
	insv r9, $0x7, $0x8, TMPFRACTGT

	bisl2 r6, TMPFRACTGT	# set the sign

	/*
	 * Now we need to separate.  CVT* won't work in the case of a
	 * >32-bit integer, so we count the integer bits and use ASHQ to
	 * shift them away.
	 */
	cmpl $0x80, r9
	blss 7f		/* if we are less than 1.0, we can avoid this */
	brw 8f
7:		
	subl3 $0x80, r9, r8

	movq TMPFRACTGT, TMPFRAC1
	/*
	 * Check for integer overflow by comparing the integer bit count.
	 * If this is the case, set V in PSL.
	 */
	cmpl r8, $0x20
	blss 3f
	bisl2 $PSL_V, S_PSL
3:
	cmpl r8, $0x38
	blss 1f
	/*
	 * In the case where we have more than 55 bits in the integer,
	 * there aren't any bits left for the fraction.  Therefore we're
	 * done here;  TMPFRAC1 is equal to TMPFRACTGT and TMPFRAC2 is 0.
	 */
	movq $0f0.0, TMPFRAC2
	jmp 9f		/* we're done, move on */
1:	
	/*
	 * We do the mod by using ASHQ to shift and truncate the bits.
	 * Before that happens, we have to arrange the bits in a quadword such
	 * that the significance increases from start to finish.
	 */

	movab TMPFRACTGT, r0
	movab TMPFRAC1, r1
	movb (r0), 7(r1)
	bisb2 $0x80, 7(r1)
	movw 2(r0), 5(r1)
	movw 4(r0), 3(r1)
	movb 7(r0), 2(r1)
	movb 6(r0), 1(r1)

	/* Calculate exactly how many bits to shift. */
	subl3 r8, $0x40, r7
	mnegl r7, r6
	ashq r6, TMPFRAC1, r0			# shift right
	ashq r7, r0, TMPFRAC2			# shift left

	/* Now put it back into a D_. */
	movab TMPFRAC2, r0
	movab TMPFRAC1, r1
 	extv $0x18, $0x7, 4(r0), (r1)
	extzv $0x7, $0x9, TMPFRACTGT, r2
	insv r2, $0x7, $0x9, (r1)

	movw 5(r0), 2(r1)
	movw 3(r0), 4(r1)
	movw 1(r0), 6(r1)

	# we have the integer in TMPFRAC1, now get the fraction in TMPFRAC2
	subd3 TMPFRAC1, TMPFRACTGT, TMPFRAC2
	jmp 9f

8:	
	/*
	 * We are less than 1.0; TMPFRAC1 should be 0, and TMPFRAC2 should
	 * be equal to TMPFRACTGT.
	 */
	movd $0f0.0, TMPFRAC1
	movd TMPFRACTGT, TMPFRAC2
9:			
	/*
	 * We're done. We can use CVTDL here, since EMODD is supposed to
	 * truncate.
	 */
	cvtdl TMPFRAC1, r4
	bsbw getaddr_byte
	movl r4, (r0)
	
	bsbw getaddr_byte
	movq TMPFRAC2, (r0)
	movd TMPFRAC2, r0		/* move this here so we can test it later */

	/* Clean up sp. */

	addl2 $0x74, sp	
	movl (sp)+, ap

	/*
	 * Now set condition codes.  We know Z == 0; C is always 0; and V
	 * is set above as necessary.  Check to see if TMPFRAC2 is
	 * negative; if it is, set N.
	 */
	tstd r0
	bgeq 1f /* branch if N == 0 */
	bisl2 $PSL_N, S_PSL
1:
	brw goback
zeroexit:
	/* Z == 1, everything else has been cleared already */
	bisl2 $PSL_Z, S_PSL
	bsbw getaddr_byte
	movl $0x0, (r0)
	bsbw getaddr_byte
	movd $0f0, (r0)
	brw goback



/* 
 * bitcnt: counts significant bits backwards in a quadword 
 * returns number of bits, unless there aren't any;
 * in that case it will return $0xffffffff
 */
ASENTRY(bitcnt, R1|R2|R3|R4|R5|R6|R7|R8|R9|R10|R11)
	/* 
	 * Our goal is to factor a common power of 2 out of each of the
	 * two factors involved in the multiplication.  Once we have that,
	 * we can multiply them as integers.  More below.
	 * Right now we are counting bits, starting from the highest octet
	 * of each (the *least* significant bit at this point!) and doing
	 * FFSes until we find a bit set.
	 */ 
	movl 4(ap), r0
	movl $0x8, r1
1:	decl r1
	addl3 r1, r0, r4
	movzbl (r4), r2
	ffs $0, $0x20, r2, r3
	bneq 2f		/* if we found a bit, Z == 0, continue */
	cmpl r1, $0x0
	jeql 3f /* if r1 is zero and there's no bit set, qw is 0 */
	jmp 1b			/* else continue with the loop */

2:	/* 
	 * We found a bit; its position in the byte is in r3, and r1 is the
	 * position of the byte in the quadword.
	 */
	subl3 r3, $0x8, r0	
	ashl $0x5, r1, r2
	addl2 r2, r0
	ret

3:	/* this quadword is 0 */
	movl $0xffffffff, r0
	ret


/*
 * The fltext_X routines separate fraction and exponent* bits. 
 * They return (via r0) the amount of bits in the fraction.
 *
 * *: exponents are left in excess-128 form
 *        D_ floating point first word:
 *         F E      7 6     0
 *        +-+--------+-------+
 * sign-> |s|exponent| fract.|  (10-3F = fraction bits)
 *        +-+--------+-------+
 *        Significance order: 0-6, 10-1F, 20-2F, 30-3F
 * 
 * The fourth argument to fltext_De is the eight extra bits for use
 * in EMOD*, et al.  If these bits are not in use, specify 0.
 */
ASENTRY(fltext_De, R0|R1|R2)
	movl 0x4(ap), r0	# r0 - addr of source
	movl 0x8(ap), r1	# r1 - addr of fraction destination

	movb (r0), (r1)
	bisb2 $0x80, (r1)+	# This is the hidden bit.

	movb 3(r0), (r1)+
	movb 2(r0), (r1)+
	movb 5(r0), (r1)+
	movb 4(r0), (r1)+
	movb 7(r0), (r1)+
	movb 6(r0), (r1)+

	/*
	 * if there are extension bits (EMOD EDIV etc.) they are 
	 * low-order
	 */
	movb 0x10(ap), (r1)	

	movl 0x4(ap), r0	# r0 - addr of source
	movl 0xc(ap), r2	# r2 - addr of exponent destination
	extzv $0x7, $0x8, (r0), (r2)		# get exponent out
	ret
