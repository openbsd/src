/*	$OpenBSD: pcb.h,v 1.5 2008/06/26 05:42:09 ray Exp $	*/
/*	$NetBSD: pcb.h,v 1.1 2003/04/26 18:39:45 fvdl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)pcb.h	5.10 (Berkeley) 5/12/91
 */

/*
 * XXXfvdl these copyrights don't really match anymore
 */

#ifndef _AMD64_PCB_H_
#define _AMD64_PCB_H_

#include <sys/signal.h>

#include <machine/segments.h>
#include <machine/tss.h>
#include <machine/fpu.h>
#include <machine/sysarch.h>

#define	NIOPORTS	1024		/* # of ports we allow to be mapped */

/*
 * Please note that the pcb_savefpu field in struct below must be
 * on a 16-byte boundary.
 */
struct pcb {
	/*
	 * XXXfvdl
	 * It's overkill to have a TSS here, as it's only needed
	 * for compatibility processes who use an I/O permission map.
	 * The pcb fields below are not in the TSS anymore (and there's
	 * not enough room in the TSS to store them all)
	 * Should just make this a pointer and allocate.
	 */
	struct	x86_64_tss pcb_tss;
	u_int64_t pcb_cr3;
	u_int64_t pcb_rsp;
	u_int64_t pcb_rbp;
	u_int64_t pcb_usersp;
	u_int64_t pcb_ldt_sel;
	struct	savefpu pcb_savefpu;	/* floating point state */
	int	pcb_cr0;		/* saved image of CR0 */
	int	pcb_flags;
	caddr_t	pcb_onfault;		/* copyin/out fault recovery */
	struct cpu_info *pcb_fpcpu;	/* cpu holding our fp state. */
	unsigned pcb_iomap[NIOPORTS/32];	/* I/O bitmap */
	struct pmap *pcb_pmap;		/* back pointer to our pmap */
};

/*    
 * The pcb is augmented with machine-dependent additional data for 
 * core dumps. For the i386, there is nothing to add.
 */     
struct md_coredump {
	long	md_pad[8];
};    

#endif /* _AMD64_PCB_H_ */
