/* $OpenBSD: coproc15.S,v 1.2 2000/03/03 00:54:46 todd Exp $ */
/* $NetBSD: coproc15.S,v 1.1 1996/01/31 23:15:33 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * coproc15.S 
 *
 * Manipulation of the CPU internal coprocessor #15 registers
 *
 * Created      : 29/11/94
 * Last updated : 28/08/95
 *
 * Based on arm/readcoproc15.S & arm/writecoproc15.S
 *
 */

lr	.req	r14
pc	.req	r15

.text
/*
 * Functions to read the internal coprocessor registers
 *
 * Currently the only registers that can be read are
 *  r0 - CPU ID
 *  r5 - Fault status
 *  r6 - Fault address
 *
 * Eventually these should be inlined.
 */

	.global	_cpu_id

_cpu_id:
	mrc	15, 0, r0, c0, c0, 0
	mov	pc, lr

	.global	_cpu_faultstatus

_cpu_faultstatus:
	mrc	15, 0, r0, c5, c0, 0
	mov	pc, lr

	.global	_cpu_faultaddress

_cpu_faultaddress:
	mrc	15, 0, r0, c6, c0, 0
	mov	pc, lr


/*
 * Functions to write the internal coprocessor registers
 *
 *
 * Currently the only registers that can be write are
 *  r1 - CPU Control
 *  r2 - TTB
 *  r3 - Domain Access Control
 *  r5 - Flush TLB
 *  r6 - Purge TLB
 *  r7 - Flush IDC
 *
 * Eventually these should be inlined.
 */
 
	.global	_cpu_control

_cpu_control:
	mcr	15, 0, r0, c1, c0, 0
	mov	pc, lr

	.global	_setttb

_setttb:
/* We need to flush the cache as it uses virtual addresses that are about to change */
	mcr	15, 0, r0, c7, c0, 0

/* Write the TTB */
	mcr	15, 0, r0, c2, c0, 0

/* If we have updated the TTB we must flush the TLB */
	mcr	15, 0, r0, c5, c0, 0

/* For good measure we will flush the IDC as well - do we need this */
	mcr	15, 0, r0, c7, c0, 0

/* Make sure that pipeline is emptied */

	mov	r0, r0
	mov	r0, r0

	mov	pc, lr

	.global	_cpu_domains

_cpu_domains:
	mcr	15, 0, r0, c3, c0, 0
	mov	pc, lr

	.global	_tlbflush

_tlbflush:
	mcr	15, 0, r0, c5, c0, 0
	mov	pc, lr

	.global	_tlbpurge

_tlbpurge:
	mcr	15, 0, r0, c6, c0, 0
	mov	pc, lr

	.global	_idcflush

_idcflush:
	mcr	15, 0, r0, c7, c0, 0
	mov	pc, lr
