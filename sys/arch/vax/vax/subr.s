/*	$OpenBSD: subr.s,v 1.26 2007/05/16 05:19:15 miod Exp $     */
/*	$NetBSD: subr.s,v 1.32 1999/03/25 00:41:48 mrg Exp $	   */

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

#include <machine/asm.h>

#include "assym.h"
#ifdef COMPAT_ULTRIX
#include <compat/ultrix/ultrix_syscall.h>
#endif

#define JSBENTRY(x)	.globl x ; .align 2 ; x :

		.text

/*
 * First entry routine from boot. This should be in a file called locore.
 */
ASENTRY(start, 0)
	bisl3	$0x80000000,r9,_esym		# End of loaded code
	pushl	$0x1f0000			# Push a nice PSL
	pushl	$to				# Address to jump to
	rei					# change to kernel stack
to:	movw	$0xfff,_panic			# Save all regs in panic
	moval	_end, r0			# Get kernel end address
	addl2	$0x3ff, r0			# Round it up
	cmpl	_esym, r0			# Compare with symbol table end
	bleq	eskip				# Symbol table not present
	addl3	_esym, $0x3ff, r0		# Use symbol end and round
eskip:
	bicl3	$0x3ff,r0,_proc0paddr		# save proc0 uarea pointer
	bicl3	$0x80000000,_proc0paddr,r0	# get phys proc0 uarea addr
	mtpr	r0,$PR_PCBB			# Save in IPR PCBB
	addl3	$USPACE,_proc0paddr,r0		# Get kernel stack top
	mtpr	r0,$PR_KSP			# put in IPR KSP
	movl	r0,_Sysmap			# SPT start addr after KSP

# Set some registers in known state
	movl	_proc0paddr,r0
	clrl	P0LR(r0)
	clrl	P1LR(r0)
	mtpr	$0,$PR_P0LR
	mtpr	$0,$PR_P1LR
	movl	$0x80000000,r1
	movl	r1,P0BR(r0)
	movl	r1,P1BR(r0)
	mtpr	r1,$PR_P0BR
	mtpr	r1,$PR_P1BR
	clrl	IFTRAP(r0)
	mtpr	$0,$PR_SCBB

# Copy the RPB to its new position
#if 1 /* compat with old bootblocks */
	tstl	(ap)				# Any arguments?
	bneq	1f				# Yes, called from new boot
	movl	r11,_boothowto			# Howto boot (single etc...)
#	movl	r10,_bootdev			# uninteresting, will complain
	movl	r8,_avail_end			# Usable memory (from VMB)
	clrl	-(sp)				# Have no RPB
	brb	2f
#endif

1:	pushl	4(ap)				# Address of old rpb
2:	calls	$1,_start			# Jump away.
	/* NOTREACHED */


/*
 * Signal handler code.
 */

		.globl	_sigcode,_esigcode
_sigcode:	pushr	$0x3f
		subl2	$0xc,sp
		movl	0x24(sp),r0
		calls	$3,(r0)
		popr	$0x3f
		chmk	$SYS_sigreturn
		chmk	$SYS_exit
		halt	
		.align	2
_esigcode:

#ifdef COMPAT_ULTRIX
		.globl	_ultrix_sigcode,_ultrix_esigcode
_ultrix_sigcode:	pushr	$0x3f
		subl2	$0xc,sp
		movl	0x24(sp),r0
		calls	$3,(r0)
		popr	$0x3f
		chmk	$ULTRIX_SYS_sigreturn
		chmk	$SYS_exit
		halt	
		.align	2
_ultrix_esigcode:
#endif

		.globl	_idsptch, _eidsptch
_idsptch:	pushr	$0x3f
		.word	0x9f16		# jsb to absolute address
		.long	_cmn_idsptch	# the absolute address
		.long	0		# the callback interrupt routine
		.long	0		# its argument
		.long	0		# ptr to correspond evcount struct
_eidsptch:

_cmn_idsptch:
		movl	(sp)+,r0	# get pointer to idspvec
		movl	8(r0),r1	# get evcount pointer
		beql	1f		# no ptr, skip increment
		incl	EC_COUNT(r1)	# increment low longword
		adwc	$0,EC_COUNT+4(r1) # add any carry to hi longword
1:		pushl	4(r0)		# push argument
		calls	$1,*(r0)	# call interrupt routine
		popr	$0x3f		# pop registers
		rei			# return from interrut

ENTRY(badaddr,R2|R3)			# Called with addr,b/w/l
		mfpr	$0x12,r0	# splhigh()
		mtpr	$0x1f,$0x12
		movl	4(ap),r2	# First argument, the address
		movl	8(ap),r1	# Sec arg, b,w,l
		pushl	r0		# Save old IPL
		clrl	r3
		movab	4f,_memtest	# Set the return address

		caseb	r1,$1,$4	# What is the size
1:		.word	1f-1b		
		.word	2f-1b
		.word	3f-1b		# This is unused
		.word	3f-1b
		
1:		movb	(r2),r1		# Test a byte
		brb	5f

2:		movw	(r2),r1		# Test a word
		brb	5f

3:		movl	(r2),r1		# Test a long
		brb	5f

4:		incl	r3		# Got machine chk => addr bad
5:		mtpr	(sp)+,$0x12
		movl	r3,r0
		ret

# Have bcopy and bzero here to be sure that system files that not gets
# macros.h included will not complain.
ENTRY(bcopy,R2|R3|R4|R5|R6)
	movl	4(ap), r0
	movl	8(ap), r1
	movzwl	12(ap), r2
	movzwl	14(ap), r6

	movc3	r2, (r0), (r1)
	
	tstl	r6
	bleq	1f
0:	movb	(r1)+, (r3)+
	movc3	$0xffff, (r1), (r3)
	sobgtr	r6, 0b
	
1:	ret	

ENTRY(bzero,R2|R3|R4|R5|R6)
	movl	4(ap), r0
	movzwl	8(ap), r1
	movzwl	10(ap), r6
	
	movc5	$0, (r0), $0, r1, (r0)
	
	tstl	r6
	bleq	1f
0:	clrb	(r3)+
	movc5	$0, (r3), $0, $0xffff, (r3)
	sobgtr	r6, 0b
	
1:	ret

# cmpc3 is sometimes emulated; we cannot use it
ENTRY(bcmp, R2);
    movl    4(ap), r2
    movl    8(ap), r1
    movl    12(ap), r0
2:  cmpb    (r2)+, (r1)+
    bneq    1f
    decl    r0
    bneq    2b
1:  ret

#ifdef DDB
/*
 * DDB is the only routine that uses setjmp/longjmp.
 */
ENTRY(setjmp, 0)
	movl	4(ap), r0
	movl	8(fp), (r0)
	movl	12(fp), 4(r0)
	movl	16(fp), 8(r0)
	addl3	fp,$28,12(r0)
	clrl	r0
	ret

ENTRY(longjmp, 0)
	movl	4(ap), r1
	movl	8(ap), r0
	movl	(r1), ap
	movl	4(r1), fp
	movl	12(r1), sp
	jmp	*8(r1)
#endif 

#
# setrunqueue/remrunqueue fast variants.
#

JSBENTRY(Setrq)
#ifdef DIAGNOSTIC
	tstl	4(r0)	# Check that process actually are off the queue
	beql	1f
	pushab	setrq
	calls	$1,_panic
setrq:	.asciz	"setrunqueue"
#endif
1:	extzv	$2,$6,P_PRIORITY(r0),r1 # get priority
	movaq	_qs[r1],r2		# get address of queue
	insque	(r0),*4(r2)		# put proc last in queue
	bbss	r1,_whichqs,1f		# set queue bit.
1:	rsb

JSBENTRY(Remrq)
	extzv	$2,$6,P_PRIORITY(r0),r1
#ifdef DIAGNOSTIC
	bbs	r1,_whichqs,1f
	pushab	remrq
	calls	$1,_panic
remrq:	.asciz	"remrunqueue"
#endif
1:	remque	(r0),r2
	bneq	1f		# Not last process on queue
	bbsc	r1,_whichqs,1f
1:	clrl	4(r0)		# saftey belt
	rsb

#
# Idle loop. Here we could do something fun, maybe, like calculating
# pi or something.
#
idle:	mtpr	$0,$PR_IPL		# Enable all types of interrupts
1:	tstl	_whichqs		# Anything ready to run?
	beql	1b			# no, continue to loop
	brb	Swtch			# Yes, goto switch again.

#
# cpu_switch, cpu_exit and the idle loop implemented in assembler 
# for efficiency. r0 contains pointer to last process.
#

#define CURPROC _cpu_info_store + CI_CURPROC

JSBENTRY(Swtch)
	clrl	CURPROC			# Stop process accounting
#bpt
	mtpr	$0x1f,$PR_IPL		# block all interrupts
	ffs	$0,$32,_whichqs,r3	# Search for bit set
	beql	idle			# no bit set, go to idle loop

	movaq	_qs[r3],r1		# get address of queue head
	remque	*(r1),r2		# remove proc pointed to by queue head
#ifdef DIAGNOSTIC
	bvc	1f			# check if something on queue
	pushab	noque
	calls	$1,_panic
noque:	.asciz	"swtch"
#endif
1:	bneq	2f			# more processes on queue?
	bbsc	r3,_whichqs,2f		# no, clear bit in whichqs
2:	clrl	4(r2)			# clear proc backpointer
	clrl	_want_resched		# we are now changing process
	movb	$SONPROC,P_STAT(r2)	# p->p_stat = SONPROC
	movl	r2,CURPROC		# set new process running
	cmpl	r0,r2			# Same process?
	bneq	1f			# No, continue
	rsb
xxd:	
1:	movl	P_ADDR(r2),r0		# Get pointer to new pcb.
	addl3	r0,$IFTRAP,pcbtrap	# Save for copy* functions.

#
# Nice routine to get physical from virtual addresses.
#
	extzv	$9,$21,r0,r1		# extract offset
	movl	*_Sysmap[r1],r2		# get pte
	ashl	$9,r2,r3		# shift to get phys address.

#
# Do the actual process switch. pc + psl are already on stack, from
# the calling routine.
#
	svpctx
	mtpr	r3,$PR_PCBB
	ldpctx
	rei

#
# the last routine called by a process.
#

ENTRY(cpu_exit,0)
	movl	4(ap),r6	# Process pointer in r6
	mtpr	$0x18,$PR_IPL	# Block almost everything
	addl3	$512,_scratch,sp # Change stack, and schedule it to be freed

	pushl	r6		
	calls	$1,_exit2

	clrl	r0		# No process to switch from
	bicl3	$0xc0000000,_scratch,r1
	mtpr	r1,$PR_PCBB
	brw	Swtch


#
# copy/fetch/store routines. 
#
	.align 2,1
ENTRY(copyin, R2|R3|R4|R5|R6)
	movl	4(ap), r0
	blss	3f		# kernel space
	movl	8(ap), r1
	brb	2f

ENTRY(copyout, R2|R3|R4|R5|R6)
	movl	8(ap), r1
	blss	3f		# kernel space
	movl	4(ap), r0
2:	movab	1f,*pcbtrap
	movzwl	12(ap), r2
	movzwl	14(ap), r6

	movc3	r2, (r0), (r1)
	
	tstl	r6
	bleq	1f
0:	movb	(r1)+, (r3)+
	movc3	$0xffff, (r1), (r3)
	sobgtr	r6,0b
	
1:	clrl	*pcbtrap
	ret

3:	movl	$EFAULT, r0
	ret

/* kcopy:  just like bcopy, except return -1 upon failure */	
ENTRY(kcopy,R2|R3|R4|R5|R6)
	movl	*pcbtrap,-(sp)
	movab	2f,*pcbtrap
	movl	4(ap), r0
	movl	8(ap), r1
	movzwl	12(ap), r2
	movzwl	14(ap), r6

	movc3	r2, (r0), (r1)
	
	tstl	r6
	bleq	1f
0:	movb	(r1)+, (r3)+
	movc3	$0xffff, (r1), (r3)
	sobgtr	r6, 0b
	
	/* 
	 * If there is a failure, trap.c will set r1 to -1, and jump
	 * to the following 2.  If not, we return 0.  We duplicate a 
	 * minuscule amount of code in the interest of speed; movc3
	 * sets r0 to 0 anyway.
	 */
1:
	movl	(sp)+,*pcbtrap
	ret
	
2:	movl	(sp)+,*pcbtrap
	movl	r1,r0
	ret

ENTRY(copyinstr,0)
	tstl	4(ap)		# is from a kernel address?
	bgeq	8f		# no, continue

6:	movl	$EFAULT,r0
	ret

ENTRY(copyoutstr,0)
	tstl	8(ap)		# is to a kernel address?
	bgeq	8f		# no, continue
	brb	6b

ENTRY(copystr,0)
8:	movl	4(ap),r4	# from
	movl	8(ap),r5	# to
	movl	16(ap),r3	# copied
	movl	12(ap),r2	# len

	bneq	1f		# nothing to copy?
	movl	$ENAMETOOLONG,r0
	tstl	r3
	beql	0f
	movl	$0,(r3)
0:	ret

1:	movab	2f,*pcbtrap

/*
 * This routine consists of two parts: One is for MV2 that doesn't have
 * locc in hardware, the other is a fast version with locc. But because
 * locc only handles <64k strings, we default to the slow version if the
 * string is longer.
 */
	cmpl	_vax_cputype,$VAX_TYP_UV2
	bneq	4f		# Check if locc emulated

9:	movl	r2,r0
7:	movb	(r4)+,(r5)+
	beql	6f		# end of string
	sobgtr	r0,7b		# no null byte in the len first bytes?
	brb 1f

6:	tstl	r3
	beql	5f
	incl	r2
	subl3	r0,r2,(r3)
5:	clrl	r0
	clrl	*pcbtrap
	ret

4:	cmpl	r2,$65535	# maxlen < 64k?
	blss	8f		# then use fast code.

	locc	$0,$65535,(r4)	# is strlen < 64k?
	beql	9b		# No, use slow code
	subl3	r0,$65535,r1	# Get string len
	brb	0f		# do the copy

8:	locc	$0,r2,(r4)	# check for null byte
	beql	1f

	subl3	r0,r2,r1	# Calculate len to copy
0:	incl	r1		# Copy null byte also
	tstl	r3
	beql	3f
	movl	r1,(r3)		# save len copied
3:	movc3	r1,(r4),(r5)
	brb	4f

1:	movl	$ENAMETOOLONG,r0
2:	movab	4f,*pcbtrap	# if we fault again, don't resume there
	subl3	8(ap),r5,r1	# did we write to the string?
	beql	3f
	decl	r5
3:	movb	$0,(r5)		# null terminate the output string
	tstl	r3
	beql	4f
	incl	r1		# null byte accounts for outlen...
	movl	r1,(r3)		# save len copied
4:	clrl	*pcbtrap
	ret

#
# data department
#
	.data

_memtest:	.long 0 ; .globl _memtest	# Memory test in progress.
pcbtrap:	.long 0x800001fc; .globl pcbtrap	# Safe place
_bootdev:	.long 0; .globl _bootdev

/*
 * Fill more than 64k of memory (used by bzero and memset).
 */
ENTRY(blkfill,R2|R3|R4|R5|R6|R7)
	movl	4(ap), r3
	movl	8(ap), r7
	movl	12(ap), r6
	jbr	2f
1:	subl2	r0, r6
	movc5	$0,(r3),r7,r0,(r3)
2:	movzwl	$65535,r0
	cmpl	r6, r0
	jgtr	1b
	movc5	$0,(r3),r7,r6,(r3)
	ret
