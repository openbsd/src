/*	$OpenBSD: process.S,v 1.5 1999/02/09 06:36:30 smurph Exp $	*/
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
#ifndef ASSEMBLER
#define ASSEMBLER
#endif

#include "assym.s"
#include "machine/asm.h"
#include "machine/psl.h"

#ifndef NBPG
#define	NBPG 4096
#endif /* NBPG */

	data
	align 4
Lsw0:
	string	"cpu_switch\n"
	align 4
swchanpanic:
	string  "switch wchan %x\n"
	align 4
swsrunpanic:
	string	"switch SRUN %x\n"

	text
	align 8
Lswchanpanic:
	or.u r2, r0, hi16(swchanpanic)
	or   r2, r2, lo16(swchanpanic)
	or   r3, r0, r9
	bsr  _panic

Lswsrunpanic:
	or.u r2, r0, hi16(swsrunpanic)
	or   r2, r2, lo16(swsrunpanic)
	or   r3, r0, r9
	bsr  _panic
/*
 * At exit of a process, do a cpu_switch for the last time.
 * The mapping of the pcb at p->p_addr has already been deleted,
 * and the memory for the pcb+stack has been freed.
 * The ipl is high enough to prevent the memory from being reallocated.
 */

ENTRY(switch_exit)
	/*
	 * Change pcb to idle u. area, i.e., set r31 to top of stack
	 * and set curpcb to point to _idle_u.
	 */
	or.u	r30, r0, hi16(_idle_u)
	or	r30, r30,lo16(_idle_u)
	addu	r31, r30, UPAGES * NBPG	/* now on idle_u stack */
	or.u	r10, r0, hi16(_curpcb)
	st	r30, r10,lo16(_curpcb)	/* curpcb = &idle_u */
	or.u	r10, r0, hi16(_curproc)
	st	r0, r10, lo16(_curproc)	/* curproc = NULL */
	bsr.n	_cpu_switch
	or	r2, r0, r10

#if 0
/*
 * When no processes are on the runq, switch
 * idles here watiing for something to come ready.
 */
LABEL(idle)
	or.u	r10, r0, hi16(_curproc)
	st	r0, r10, lo16(_curproc)	/* curproc = NULL */

	bsr.n	_setipl			/* unblock all interrupts */
	or	r2, r0, 0
					/* spin reading whichqs until != 0 */
1:
	or.u	r10, r0, hi16(_whichqs)
	ld	r11, r10,lo16(_whichqs) 
	bcnd	eq0, r11, 1b
	bsr.n	_setipl
	or	r2, r0, IPL_HIGH	/* block all ints */
	br	Lsw1
#endif /* 0 */
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

	or.u	r10, r0, hi16(_curpcb)
	ld	r10,r10, lo16(_curpcb)
	st	r1, r10, PCB_PC		/*  save r1 in pcb */
	st.d	r14,r10, PCB_R14
	st.d	r16,r10, PCB_R16
	st.d	r18,r10, PCB_R18
	st.d	r20,r10, PCB_R20
	st.d	r22,r10, PCB_R22
	st.d	r24,r10, PCB_R24
	st.d	r26,r10, PCB_R26
	st.d	r28,r10, PCB_R28
	st.d	r30,r10, PCB_R30	/* save frame pointer & stack pointer */

	or	r14,r10, 0		/* save r10 in r14 */

	bsr	_getipl

	st	r2, r14, PCB_IPL	/* save ipl in pcb */

	or.u	r11, r0, hi16(_curproc)
	ld	r11,r11, lo16(_curproc)

	or.u	r12, r0, hi16(_lastproc)
	st	r11, r12, lo16(_lastproc)/* lastproc = curproc */

	or.u	r11, r0, hi16(_curproc)
	st	r0, r11, lo16(_curproc)	/* curproc = NULL */

	or.u	r11, r0, hi16(_curpcb)
	st	r0, r11, lo16(_curpcb)	/* curpcb = NULL */

Lidleloop:

	/*
	 * Find the highest-priority queue that isn't empty,
	 * then take the first proc from that queue.
	 */

	or.u	r7, r0, hi16(_whichqs)
	ld	r7, r7, lo16(_whichqs)

	bcnd	ne0, r7, Ldoneloop

Lloopchk:				/* if whichqs is zero, keep checking */
	bsr.n	_setipl			/* enable all ints */	
	or	r2, r0, 0

	br	Lidleloop

Ldoneloop:

	bsr.n	_setipl			/* disable ints */
	or	r2, r0, IPL_HIGH

	or.u	r7, r0, hi16(_whichqs)	/* reload whichqs */
	ld	r7, r7, lo16(_whichqs)

	bcnd	eq0, r7, Lloopchk	/* keep spinning for whichqs to be !=0*/

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
	st	r8, r12, P_BACK /* p->p_forw->p_back = q (p->p_back) */
	st	r12, r8, P_FORW /* q->p_forw = p->p_forw */
	lda.d	r8,  r7[r6]	/* reload r8 with qs[ff1(whichqs)] */
	ld	r12, r8, P_FORW /* q->p_forw */
	cmp	r12, r12, r8	/* q == q->p_forw; anyone left on queue? */
	bb1	ne,  r12, Lsw2	/* yes, skip clearing bit in whichqs  */

	or	r12, r0, 1	/* r12 is 1 now */
      1:bcnd	eq0, r6, 2f
	mak	r12, r12, 0<1>	/* shift left by 1 */
	br.n	1b
	subu	r6,  r6, 1	/* keep doing this while r6 != 0 */
      2:
	/*
	 * NOTE: we could have just used "mak	r12, r12, r6" instead of the
	 * loop above. But that will break if NQS is made > 32. I can use
	 * preprocessor to do the right thing, but that means I have to
	 * include sys/proc.h in this file. XXX nivas
	 */
	or.u	r7,  r0, hi16(_whichqs)
	ld	r8,  r7, lo16(_whichqs)
	and.c	r8,  r8, r12	/* whichqs &= ~the bit */
	st	r8,  r7, lo16(_whichqs)	/* reset bit in whichqs */
Lsw2:
	ld	r2, r9, P_WCHAN
	bcnd	ne0, r2, Lswchanpanic
	ld.b	r2, r9, P_STAT
	cmp	r2, r2, SRUN
	bb1	ne, r2, Lswsrunpanic

	or.u	r11, r0, hi16(_want_resched)
	st	r0, r11, lo16(_want_resched) 	/* clear want_resched */

	or.u	r11, r0, hi16(_curproc)
	st	r9,  r11,lo16(_curproc)		/* curproc = p */
	
#ifdef notyet
	or.u	r2, r0, hi16(_lastproc)
	ld	r2, r2, lo16(_lastproc)
#endif /* notyet */

	/* huh??? */
	or.u	r10, r0, hi16(_curpcb)
	ld	r10,r10, lo16(_curpcb)

#ifdef notyet
	cmp	r2, r2, r9
	bb1	eq, r2, Lswsameproc
#endif /* notyet */

	/*  r9 is curproc */
	st	r0, r9, P_BACK			/* p->p_back = 0 */
	ld	r3, r9, P_ADDR
	or.u	r10, r0, hi16(_curpcb)
	st	r3, r10, lo16(_curpcb)		/* curpcb = p->p_addr */

	/* see if pmap_activate needs to be called */
	ld	r2, r9, P_VMSPACE	/* vmspace = p->p_vmspace */
	addu	r2, r2, VM_PMAP		/* pmap = &vmspace.vm_pmap */
#ifdef notyet 
	ld	r5, r2, PM_STCHG	/* pmap->st_changed? */
	bcnd	eq0, r5, Lswnochg	/* no, skip */
#endif /* notyet */
	or	r14, r0, r9		/* save p in r14 */
	subu	r31, r31,48
	bsr	_pmap_activate		/* pmap_activate(pmap, pcb) */
	addu	r31, r31,48
	or	r9, r0, r14		/* restore p saved in r14 */

Lswnochg:
	or.u	r31, r0, hi16(_intstack_end)
	or	r31,r31, lo16(_intstack_end)/* now goto a tmp stack for NMI */
	subu	r31, r31,48
	bsr.n    _load_u_area		    /* load_u_area(p) */
	or	r2, r0, r9
	addu	r31, r31,48
					/* flush tlb of any user addresses */
	or	r2, r0, 0
	or	r3, r0, 0
	subu	r31, r31,48
	bsr.n	_cmmu_flush_tlb
	or	r4, r0, 0xffff		/* cmmu_flush_tlb flushes entire tlb */
					/* for sizes > 4096		     */
	addu	r31, r31,48
	or.u	r10, r0,  hi16(_curpcb)
	ld	r10, r10,  lo16(_curpcb)
	/* XXX Is this correct/necessary? */
	st	r10, r14, P_ADDR	/* p->p_addr = curpcb; restore p_addr */

	/* restore from the current context */
	ld	r1,r10,  PCB_PC	
	ld.d	r14,r10, PCB_R14
	ld.d	r16,r10, PCB_R16
	ld.d	r18,r10, PCB_R18
	ld.d	r20,r10, PCB_R20
	ld.d	r22,r10, PCB_R22
	ld.d	r24,r10, PCB_R24
	ld.d	r26,r10, PCB_R26
	ld.d	r28,r10, PCB_R28
	ld.d	r30,r10, PCB_R30	/* restore frame pointer & stack */

/* XXX should we postpone restoring stack till after ipl is restored? The
stack access could fault */
Lswsameproc:
	subu	r31,r31,48
	st	r1, r31,36		/* save r1 on stack */
	ld	r2, r10, PCB_IPL	/* restore interrupt mask */
	bsr	_setipl			/* restore ipl */
	ld	r1, r31,36		/* restore r1 from stack */
	addu	r31,r31,48
	jmp.n	r1
	or	r2, r0, 1		/* return 1 (for alternate returns) */

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* get the spl mask */
	subu	r31,r31,48		/* allocate stack for r1 and args */
	st	r1,r31,36		/* save return address */
	st	r2,r31,32		/* save r2 */
	bsr	_getipl			/* get the current interrupt mask */
	ld	r1,r31,36		/* recover return address */
	ld	r10,r31,32		/* recover r2 into r10 */
	addu	r31,r31,48		/* put stack pointer back */
	st	r1, r10, PCB_PC		/* save return address */
	st.d	r14,r10, PCB_R14
	st.d	r16,r10, PCB_R16
	st.d	r18,r10, PCB_R18
	st.d	r20,r10, PCB_R20
	st.d	r22,r10, PCB_R22
	st.d	r24,r10, PCB_R24
	st.d	r26,r10, PCB_R26
	st.d	r28,r10, PCB_R28
	st.d	r30,r10, PCB_R30	/* save frame pointer and sp */
	st	r2, r10, PCB_IPL	/* save interrupt mask */
	jmp.n	r1
	or	r2,r0,r0
