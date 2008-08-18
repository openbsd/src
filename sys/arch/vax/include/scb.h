/*	$OpenBSD: scb.h,v 1.10 2008/08/18 23:19:24 miod Exp $	*/
/*	$NetBSD: scb.h,v 1.11 2000/07/10 09:14:34 ragge Exp $	*/

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
#ifndef _VAX_SCB_H
#define	_VAX_SCB_H

/*
 * Definition of the System Control Block. More about it can be
 * found in the Vax Architecture Reference Manual, section 6.6.
 */
struct scb {
	void	*scb_unused;	/* First unused vector */
	void	*scb_mcheck;
	void	*scb_kspinv;
	void	*scb_powfail;
	void	*scb_privinst;	/* 10 Privileged Instruction fault */
	void	*scb_xfcinst;
	void	*scb_resop;
	void	*scb_resad;
	void	*scb_accessv;	/* 20 Access Control violation fault */
	void	*scb_transinv;
	void	*scb_trace;
	void	*scb_breakp;
	void	*scb_compat;	/* 30 Compatibility instruction fault */
	void	*scb_arith;
	void	*scb_unused1;
	void	*scb_unused2;
	void	*scb_chmk;	/* 40 CHMK */
	void	*scb_chme;
	void	*scb_chms;
	void	*scb_chmu;
	void	*scb_sbisilo;	/* 50 SBI Silo compare */
	void	*scb_cmrd;
	void	*scb_sbialert;
	void	*scb_sbifault;
	void	*scb_memwtimo;	/* 60 Memory write timeout */
	void	*scb_unused3;
	void	*scb_unused4;
	void	*scb_unused5;
	void	*scb_unused6;	/* 70 unused */
	void	*scb_unused7;
	void	*scb_unused8;
	void	*scb_unused9;
	void	*scb_unused10;	/* 80 unused */
	void	*scb_softint1;
	void	*scb_softint2;
	void	*scb_softint3;
	void	*scb_softint4;	/* 90 Software interrupt level 4 */
	void	*scb_softint5;
	void	*scb_softint6;
	void	*scb_softint7;
	void	*scb_softint8;	/* A0 Software interrupt level 8 */
	void	*scb_softint9;
	void	*scb_softinta;
	void	*scb_softintb;
	void	*scb_softintc;	/* B0 Software interrupt level C */
	void	*scb_softintd;
	void	*scb_softinte;
	void	*scb_softintf;
	void	*scb_timer;	/* C0 Interval timer */
	void	*scb_unused11;
	void	*scb_unused12;
	void	*scb_unused13;
	void	*scb_unused14;	/* D0 Unused */
	void	*scb_unused15;
	void	*scb_unused16;
	void	*scb_unused17;
	void	*scb_unused18;	/* E0 Unused */
	void	*scb_unused19;
	void	*scb_unused20;
	void	*scb_unused21;
	void	*scb_csrint;
	void	*scb_cstint;	/* F0 Console storage transmit interrupt */
	void	*scb_ctrint;
	void	*scb_cttint;
	struct	ivec_dsp *scb_nexvec[4][16];	/* Nexus interrupt vectors */
};

#define	SCB_KSTACK	0
#define	SCB_ISTACK	1

#define vecnum(bus, ipl, tr) (256+(ipl-0x14)*64+tr*4+bus*256)

/*
 * This struct is used when setting up interrupt vectors dynamically.
 * It puts a opaque 32 bit quantity on the stack and also has a placeholder
 * for evcount structure.
 */
struct ivec_dsp {
	char	pushr;		/* pushr */
	char	pushrarg;	/* $0x3f */
	char	jsb;
	char	mode;
	long	displacement;
	void	(*hoppaddr)(void *);
	void	*pushlarg;
	struct	evcount *ev;
};

#ifdef _KERNEL
extern	const struct ivec_dsp idsptch;
extern	struct scb *scb;
extern	struct ivec_dsp *scb_vec;

extern	paddr_t scb_init(paddr_t);
extern	int scb_vecref(int *, int *);
extern	void scb_fake(int, int);
extern	void scb_stray(void *);
extern	void scb_vecalloc(int, void(*)(void *), void *, int, struct evcount *);
#endif /* _KERNEL */

#endif /* _VAX_SCB_H */
