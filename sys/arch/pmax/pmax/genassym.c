/*	$NetBSD: genassym.c,v 1.9 1996/04/07 14:27:00 jonathan Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)genassym.c	8.2 (Berkeley) 9/23/93
 */

#include <stdio.h>
#include <stddef.h>
#include <sys/param.h>
#include <sys/buf.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/mbuf.h>
#include <sys/user.h>

#include <machine/reg.h>

#define	def(N,V)	printf("#define\t%s %d\n", N, V)
#define	defx(N,V)	printf("#define\t%s 0x%lx\n", N, V)
#define	off(N,S,M)	def(N, (int)offsetof(S, M))

int
main()
{

	off("P_FORW", struct proc, p_forw);
	off("P_BACK", struct proc, p_back);
	off("P_PRIORITY", struct proc, p_priority);
	off("P_ADDR", struct proc, p_addr);

	off("P_UPTE", struct proc, p_md.md_upte);
	off("U_PCB_REGS", struct user, u_pcb.pcb_regs);

	off("U_PCB_FPREGS", struct user, u_pcb.pcb_regs[F0]);
	off("U_PCB_CONTEXT", struct user, u_pcb.pcb_context);
	off("U_PCB_ONFAULT", struct user, u_pcb.pcb_onfault);
	off("U_PCB_SEGTAB", struct user, u_pcb.pcb_segtab);

	defx("VM_MIN_ADDRESS", VM_MIN_ADDRESS);
	defx("VM_MIN_KERNEL_ADDRESS", VM_MIN_KERNEL_ADDRESS);

	off("V_SWTCH", struct vmmeter, v_swtch);

	def("SIGILL", SIGILL);
	def("SIGFPE", SIGFPE);
	exit(0);
}
