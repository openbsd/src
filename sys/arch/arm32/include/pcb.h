/* $NetBSD: pcb.h,v 1.2 1996/03/13 21:08:36 mark Exp $ */

/*
 * Copyright (c) 1994 Mark Brinicombe.
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
 *	This product includes software developed by the RiscBSD team.
 * 4. The name "RiscBSD" nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY RISCBSD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL RISCBSD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef	_ARM32_PCB_H_
#define	_ARM32_PCB_H_

#include <machine/pte.h>
#include <machine/fp.h>

struct pcb {
	pd_entry_t	*pcb_pagedir;		/* PT hooks */
	u_int	pcb_flags;			/* Flags */
	u_int	pcb_spsr;
	u_int	pcb_r0;				/* Space for register dump */
	u_int	pcb_r1;
	u_int	pcb_r2;
	u_int	pcb_r3;
	u_int	pcb_r4;
	u_int	pcb_r5;
	u_int	pcb_r6;
	u_int	pcb_r7;
	u_int	pcb_r8;				/* used */
	u_int	pcb_r9;				/* used */
	u_int	pcb_r10;			/* used */
	u_int	pcb_r11;			/* used */
	u_int	pcb_r12;			/* used */
	u_int	pcb_sp;				/* used */
	u_int	pcb_lr;
	u_int	pcb_pc;
	u_int	pcb_und_sp;
	caddr_t	pcb_onfault;			/* On fault handler */
	struct	fp_state pcb_fpstate; 		/* Floating Point state */
};

/*
 * No additional data for core dumps.
 */
struct md_coredump {
	int	md_empty;
};

#ifdef _KERNEL
extern struct pcb *curpcb;
#endif	/* _KERNEL */

#endif	/* _ARM32_PCB_H_ */

/* End of pcb.h */
