/*	$OpenBSD: process.S,v 1.2 2004/06/13 21:49:17 niklas Exp $	*/
/*
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
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
 *
 */

#include "assym.h"
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/intr.h>

	data
	align	4
ASLOCAL(swchanpanic)
	string	"switch wchan %x\0"
	align	4
ASLOCAL(swsrunpanic)
	string	"switch SRUN %x\0"
#ifdef DEBUG
	align	4
ASLOCAL(boguspsr)
	string	"Invalid PSR in idle loop 0x%x\n\0"
#endif

	text
	align	8
ASLOCAL(Lswchanpanic)
	or.u	r2, r0, hi16(_ASM_LABEL(swchanpanic))
	or	r2, r2, lo16(_ASM_LABEL(swchanpanic))
	bsr.n	_C_LABEL(panic)
	 or	r3, r0, r9

ASLOCAL(Lswsrunpanic)
	or.u	r2, r0, hi16(_ASM_LABEL(swsrunpanic))
	or	r2, r2, lo16(_ASM_LABEL(swsrunpanic))
	bsr.n	_C_LABEL(panic)
	 or	r3, r0, r9

/*
 * At exit of a process, do a cpu_switch for the last time.
 * The mapping of the pcb at p->p_addr has already been deleted,
 * and the memory for the pcb+stack has been freed.
 * The ipl is high enough to prevent the memory from being reallocated.
 * switch_exit(proc * p)
 */

ENTRY(switch_exit)
	/*
	 * Change pcb to idle u. area, i.e., set r31 to top of stack
	 * and set curpcb to point to _idle_u.  r2 contains proc *p.
	 */
	or.u	r30, r0, hi16(_C_LABEL(idle_u))
	or	r30, r30,lo16(_C_LABEL(idle_u))
	addu	r31, r30, USIZE			/* now on idle_u stack */
	or.u	r10, r0, hi16(_C_LABEL(curpcb))
	st	r30, r10,lo16(_C_LABEL(curpcb))		/* curpcb = &idle_u */
	or.u	r10, r0, hi16(_C_LABEL(curproc))
	st	r0, r10, lo16(_C_LABEL(curproc))	/* curproc = NULL */

	/* Schedule the vmspace and stack to be freed. */
	bsr.n	_C_LABEL(exit2)
	 subu	r31, r31, 48		/* allocate stack  */
	addu	r31, r31, 48		/* restore stack */
	bsr	_C_LABEL(cpu_switch)	/* goto final switch */

/*
 * cpu_switch()
 * XXX - Arg 1 is a proc pointer (curproc) but this doesn't use it.
 * XXX - how about using stack for saving spl and last proc?
 * XXX rewrite this whole mess in C nivas
 */
ENTRY(cpu_switch)

	/*
	 * Save state of previous process in its pcb.
	 */
	or.u	r2, r0, hi16(_C_LABEL(curpcb))
	ld	r2, r2, lo16(_C_LABEL(curpcb))
	st	r1, r2, PCB_PC		/* save return address */
	bsr	_ASM_LABEL(__savectx)
	/* note that we don't need to recover r1 at this point */

	or.u	r11, r0, hi16(_C_LABEL(curproc))
	ld	r2, r11, lo16(_C_LABEL(curproc))
	bcnd	eq0, r2, 1f

	bsr.n	_C_LABEL(pmap_deactivate)
	 subu	r31, r31,48
	addu    r31, r31,48
	or.u	r11, r0, hi16(_C_LABEL(curproc))

1:
	st	r0, r11, lo16(_C_LABEL(curproc))	/* curproc = NULL */

ASLOCAL(Lidleloop)

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */

	or.u	r7, r0, hi16(_C_LABEL(whichqs))
	ld	r7, r7, lo16(_C_LABEL(whichqs))

	bcnd	ne0, r7, _ASM_LABEL(Ldoneloop)

ASLOCAL(Lloopchk)			/* if whichqs is zero, keep checking */
	bsr.n	_C_LABEL(setipl)	/* unmask all ints... */
	 or	r2, r0, IPL_NONE

	ldcr	r2, PSR
	bb0	PSR_INTERRUPT_DISABLE_BIT, r2, 1f
#ifdef DEBUG
	or	r3, r2, r0
	or.u	r2, r0, hi16(_ASM_LABEL(boguspsr))
	bsr.n	_C_LABEL(printf)
	 or	r2, r2, lo16(_ASM_LABEL(boguspsr))
	ldcr	r2, PSR
#endif
	clr	r2, r2, 1<PSR_INTERRUPT_DISABLE_BIT>	/* ...and enable them */
	stcr	r2, PSR
	FLUSH_PIPELINE
1:
	br	_ASM_LABEL(Lidleloop)

ASLOCAL(Ldoneloop)

	bsr.n	_C_LABEL(setipl)	/* disable ints */
	 or	r2, r0, IPL_HIGH

	or.u	r7, r0, hi16(_C_LABEL(whichqs))	/* reload whichqs */
	ld	r7, r7, lo16(_C_LABEL(whichqs))

	bcnd	eq0, r7, _ASM_LABEL(Lloopchk)	/* keep spinning for whichqs to be != 0 */

	xor	r6, r6, r6		/* set r6 to 0 */
1:	bb1	0,  r7, 2f		/* if rightmost bit set, done */
	extu	r7, r7, 0<1>		/* else, right shift whichqs, */
	br.n	1b			/* increment r6, and repeat */
	 addu	r6, r6, 1
2:
	or.u	r7, r0, hi16(_qs)
	or	r7, r7, lo16(_qs)

	/*
	 * Need to make
	 *	p->p_forw->p_back = p->p_back and
	 *	p->p_back->p_forw = p->p_forw where
	 *	p is q->p_forw.
	 * Remember that q->p_forw == p and p->p_back == q.
	 */

	lda.d	r8,  r7[r6]	/* r8 = &qs[ff1(whichqs)] */
	ld	r9,  r8, P_FORW	/* r8 is q, r9 is p */

	ld	r12, r9, P_FORW	/* r12 = p->p_forw */
	st	r8, r12, P_BACK	/* p->p_forw->p_back = q (p->p_back) */
	st	r12, r8, P_FORW	/* q->p_forw = p->p_forw */
	lda.d	r8,  r7[r6]	/* reload r8 with qs[ff1(whichqs)] */
	ld	r12, r8, P_FORW	/* q->p_forw */
	cmp	r12, r12, r8	/* q == q->p_forw; anyone left on queue? */
	bb1	ne,  r12, 3f	/* yes, skip clearing bit in whichqs  */

	or	r12, r0, 1	/* r12 is 1 now */
1:	bcnd	eq0, r6, 2f
	mak	r12, r12, 0<1>	/* shift left by 1 */
	br.n	1b
	 subu	r6,  r6, 1	/* keep doing this while r6 != 0 */
2:
	/*
	 * NOTE: we could have just used "mak r12, r12, r6" instead of the
	 * loop above. But that will break if NQS is made > 32. I can use
	 * preprocessor to do the right thing, but that means I have to
	 * include sys/proc.h in this file. XXX nivas
	 */
	or.u	r7,  r0, hi16(_C_LABEL(whichqs))
	ld	r8,  r7, lo16(_C_LABEL(whichqs))
	and.c	r8,  r8, r12	/* whichqs &= ~the bit */
	st	r8,  r7, lo16(_C_LABEL(whichqs))
3:
	ld	r2, r9, P_WCHAN
	bcnd	ne0, r2, _ASM_LABEL(Lswchanpanic)
	ld.b	r2, r9, P_STAT
	cmp	r2, r2, SRUN
	bb1	ne, r2, _ASM_LABEL(Lswsrunpanic)

	or.u	r11, r0, hi16(_C_LABEL(want_resched))
	st	r0, r11, lo16(_C_LABEL(want_resched))	/* clear want_resched */

	or.u	r11, r0, hi16(_C_LABEL(curproc))
	st	r9,  r11,lo16(_C_LABEL(curproc))	/* curproc = p */
	or	r2, r0, SONPROC
	st.b	r2, r9, P_STAT

	/*  r9 is curproc */
	st	r0, r9, P_BACK			/* p->p_back = 0 */
	ld	r3, r9, P_ADDR
	or.u	r10, r0, hi16(_C_LABEL(curpcb))
	st	r3, r10, lo16(_C_LABEL(curpcb))		/* curpcb = p->p_addr */

	/* pmap_activate() the process' pmap */
	or	r2, r0, r9		/* r2 = p */
	or	r14, r0, r9		/* save p in r14 */
	bsr.n	_C_LABEL(pmap_activate)
	 subu	r31, r31,48
	addu    r31, r31,48
	or	r9, r0, r14		/* restore p saved in r14 */

	or.u	r31, r0, hi16(_ASM_LABEL(intstack_end))
	or	r31,r31, lo16(_ASM_LABEL(intstack_end))
	subu	r31, r31,48
	bsr.n    _C_LABEL(load_u_area)
	 or	r2, r0, r9
	addu	r31, r31,48

	or.u	r10, r0,  hi16(_C_LABEL(curpcb))
	ld	r10, r10,  lo16(_C_LABEL(curpcb))
	/* XXX Is this correct/necessary? */
	st	r10, r14, P_ADDR	/* p->p_addr = curpcb; restore p_addr */

	/* restore from the current context */
	ld	r2, r10, PCB_FCR62
	ld	r3, r10, PCB_FCR63
	fstcr	r2, fcr62
	fstcr	r3, fcr63
	ld	r1, r10, PCB_PC
	ld	r14,r10, PCB_R14
	ld	r15,r10, PCB_R15
	ld	r16,r10, PCB_R16
	ld	r17,r10, PCB_R17
	ld	r18,r10, PCB_R18
	ld	r19,r10, PCB_R19
	ld	r20,r10, PCB_R20
	ld	r21,r10, PCB_R21
	ld	r22,r10, PCB_R22
	ld	r23,r10, PCB_R23
	ld	r24,r10, PCB_R24
	ld	r25,r10, PCB_R25
	ld	r26,r10, PCB_R26
	ld	r27,r10, PCB_R27
	ld	r28,r10, PCB_R28
	ld	r29,r10, PCB_R29
	ld	r30,r10, PCB_R30	/* restore frame pointer & stack */
	ld	r31,r10, PCB_SP

/* XXX
 * Should we postpone restoring stack till after ipl is restored?
 * The stack access could fault
 */
	subu	r31,r31,48
	st	r1, r31,36		/* save r1 on stack */
	bsr.n	_C_LABEL(setipl)
	 ld	r2, r10, PCB_IPL	/* restore interrupt mask */
	ld	r1, r31,36		/* restore r1 from stack */
	addu	r31,r31,48
	jmp.n	r1
	 or	r2, r0, 1		/* return 1 (for alternate returns) */

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/*
	 * Save preserved general register set.
	 */
	st	r1,  r2,  PCB_PC		/* save return address */
ASENTRY(__savectx)
	st	r14, r2,  PCB_R14
	st	r15, r2,  PCB_R15
	st	r16, r2,  PCB_R16
	st	r17, r2,  PCB_R17
	st	r18, r2,  PCB_R18
	st	r19, r2,  PCB_R19
	st	r20, r2,  PCB_R20
	st	r21, r2,  PCB_R21
	st	r22, r2,  PCB_R22
	st	r23, r2,  PCB_R23
	st	r24, r2,  PCB_R24
	st	r25, r2,  PCB_R25
	st	r26, r2,  PCB_R26
	st	r27, r2,  PCB_R27
	st	r28, r2,  PCB_R28
	st	r29, r2,  PCB_R29
	st	r30, r2,  PCB_R30	/* save frame pointer & stack pointer */
	st	r31, r2,  PCB_SP

	/*
	 * Get the current spl.
	 * We need to save r1 on the stack because we don't know if we were
	 * called as savectx or __savectx.
	 */
	subu	r31, r31, 40		/* allocate stack for r1 and args */
	st	r1,  r31, 32
	bsr.n	_C_LABEL(getipl)	/* get the current interrupt mask */
	 or	r14, r0,  r2
	st	r2,  r14, PCB_IPL	/* save interrupt mask */
	ld	r1,  r31, 32		/* recover return address */
	addu	r31, r31, 40		/* put stack pointer back */

	/*
	 * Save FP state.
	 */
	fldcr	r2,  fcr62
	fldcr	r3,  fcr63
	st	r2,  r14, PCB_FCR62
	jmp.n	r1
	 st	r3,  r14, PCB_FCR63
