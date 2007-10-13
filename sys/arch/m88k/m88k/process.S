/*	$OpenBSD: process.S,v 1.18 2007/10/13 12:54:45 miod Exp $	*/
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

/*
 * void cpu_switchto(struct proc *oldproc, struct proc *newproc)
 *
 * Switch context from oldproc to newproc. oldproc may be NULL if there is
 * no need to save the current context.
 */
ENTRY(cpu_switchto)
	/*
	 * Save state of previous process in its pcb if required.
	 */
	bcnd	eq0, r2, 1f

	ldcr	r2,  CPU
	ld	r2,  r2,  CI_CURPCB
	st	r1,  r2,  PCB_PC		/* save return address */
	bsr	_ASM_LABEL(__savectx)
	/* note that we don't need to recover r1 at this point */

1:
	ldcr	r11, CPU
	st	r3,  r11, CI_CURPROC			/* curproc = newproc */

	or	r2,  r0,  SONPROC
	st.b	r2,  r3,  P_STAT
#ifdef MULTIPROCESSOR
	st	r11, r3,  P_CPU				/* p->p_cpu = curcpu */
#endif
	ld	r2,  r3,  P_ADDR
	st	r2,  r11, CI_CURPCB			/* curpcb = p->p_addr */

	/* pmap_activate() the process' pmap */
	bsr.n	_C_LABEL(pmap_activate)
	 or	r2, r0, r3

	ldcr	r10,  CPU
	ld	r10,  r10, CI_CURPCB

	/* restore from the current context */
	ld	r2,  r10, PCB_FCR62
	ld	r3,  r10, PCB_FCR63
	fstcr	r2,  fcr62
	fstcr	r3,  fcr63
	ld	r15, r10, PCB_R15
	ld	r16, r10, PCB_R16
	ld	r17, r10, PCB_R17
	ld	r18, r10, PCB_R18
	ld	r19, r10, PCB_R19
	ld	r20, r10, PCB_R20
	ld	r21, r10, PCB_R21
	ld	r22, r10, PCB_R22
	ld	r23, r10, PCB_R23
	ld	r24, r10, PCB_R24
	ld	r25, r10, PCB_R25
	ld	r26, r10, PCB_R26
	ld	r27, r10, PCB_R27
	ld	r28, r10, PCB_R28
	ld	r29, r10, PCB_R29
	ld	r30, r10, PCB_R30	/* restore frame pointer & stack */
	ld	r31, r10, PCB_SP
	ld	r1,  r10, PCB_PC
	jmp.n	r1
	 ld	r14, r10, PCB_R14

/*
 * savectx(pcb)
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/*
	 * Save preserved general register set.
	 */
	st	r1,  r2,  PCB_PC		/* save return address */
ASLOCAL(__savectx)
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
	 * Save FP state.
	 */
	fldcr	r4,  fcr62
	fldcr	r5,  fcr63
	st	r4,  r2, PCB_FCR62
	jmp.n	r1
	 st	r5,  r2, PCB_FCR63
