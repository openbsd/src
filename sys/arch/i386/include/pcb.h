/*	$OpenBSD: pcb.h,v 1.11 2004/02/01 19:05:23 deraadt Exp $	*/
/*	$NetBSD: pcb.h,v 1.21 1996/01/08 13:51:42 mycroft Exp $	*/

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 * Intel 386 process control block
 */

#ifndef _I386_PCB_H_
#define _I386_PCB_H_

#include <sys/signal.h>

#include <machine/segments.h>
#include <machine/tss.h>
#include <machine/npx.h>
#include <machine/sysarch.h>

#define	NIOPORTS	1024		/* # of ports we allow to be mapped */

struct pcb {
	struct	i386tss pcb_tss;
#define	pcb_cr3	pcb_tss.tss_cr3
#define	pcb_esp	pcb_tss.tss_esp
#define	pcb_ebp	pcb_tss.tss_ebp
#define	pcb_cs	pcb_tss.tss_cs
#define	pcb_fs	pcb_tss.tss_fs
#define	pcb_gs	pcb_tss.tss_gs
#define	pcb_ldt_sel	pcb_tss.tss_ldt
	int	pcb_tss_sel;
	union	descriptor *pcb_ldt;	/* per process (user) LDT */
	int	pcb_ldt_len;		/*      number of LDT entries */
	int	pcb_cr0;		/* saved image of CR0 */
	int	pcb_pad[2];		/* savefpu on 16-byte boundary */
	union	savefpu pcb_savefpu;	/* floating point state for FPU */
	struct	emcsts pcb_saveemc;	/* Cyrix EMC state */
/*
 * Software pcb (extension)
 */
	caddr_t	pcb_onfault;		/* copyin/out fault recovery */
	int	vm86_eflags;		/* virtual eflags for vm86 mode */
	int	vm86_flagmask;		/* flag mask for vm86 mode */
	void	*vm86_userp;		/* XXX performance hack */
	struct pmap *pcb_pmap;		/* back pointer to our pmap */
	u_long	pcb_iomap[NIOPORTS/32];	/* I/O bitmap */
	u_char	pcb_iomap_pad;	/* required; must be 0xff, says intel */
};

/*    
 * The pcb is augmented with machine-dependent additional data for 
 * core dumps. For the i386, there is nothing to add.
 */     
struct md_coredump {
	long	md_pad[8];
};    

#ifdef _KERNEL
struct pcb *curpcb;		/* our current running pcb */
#endif

#endif /* _I386_PCB_H_ */
