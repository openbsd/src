/*	$NetBSD: mbareg.h,v 1.2 1995/10/20 13:43:44 ragge Exp $ */
/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden
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
 *      This product includes software developed at Ludd, University of Lule}.
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

/* mbareg.h - 940320/ragge */

struct mba_device {
	u_int	pad1;
	u_int	md_ds;		/* unit status */
	u_int	pad2[4];
	u_int	md_dt;		/* unit type */
	u_int	pad3[25];
};

struct mba_regs {
	u_int	mba_csr;
	u_int	mba_cr;
	u_int	mba_sr;
	u_int	mba_var;
	u_int	mba_bc;
	u_int	mba_dr;
	u_int	mba_smr;
	u_int	mba_car;
	u_int	utrymme[248];
	struct	mba_device mba_md[8];	/* unit specific regs */
	u_int	mba_map[256];
};

/*
 * Different states which can be on massbus.
 */
/* Write to mba_cr */
#define	MBACR_IBC	0x10
#define	MBACR_MMM	0x8
#define	MBACR_IE	0x4
#define	MBACR_ABORT	0x2
#define	MBACR_INIT	0x1

/* Read from mba_sr: */
#define	MBASR_DTBUSY	0x80000000
#define	MBASR_CRD	0x20000000
#define	MBASR_CBHUNG	0x800000
#define MBASR_PGE	0x80000
#define	MBASR_NED	0x40000		/* NonExistent Drive */
#define	MBASR_MCPE	0x20000		/* Massbuss Control Parity Error */
#define	MBASR_ATTN	0x10000		/* Attention from Massbus */
#define	MBASR_SPE	0x4000		/* Silo Parity Error */
#define MBASR_DTCMP	0x2000		/* Data Transfer CoMPleted */
#define MBASR_DTABT	0x1000		/* Data Transfer ABorTed */
#define MBASR_DLT	0x800		/* Data LaTe */
#define MBASR_WCKUE	0x400		/* Write check upper error */
#define	MBASR_WCKLE	0x200		/* Write check lower error */
#define	MBASR_MXE	0x100		/* Miss transfer error */
#define	MBASR_MBEXC	0x80		/* Massbuss exception */
#define	MBASR_MDPE	0x40		/* Massbuss data parity error */
#define	MBASR_MAPPE	0x20		/* Page frame map parity error */
#define	MBASR_INVMAP	0x10		/* Invalid map */
#define MBASR_ERR_STAT	0x8		/* Error status */
#define	MBASR_NRSTAT	0x2		/* No Response status */

/* Definitions in mba_device md_ds */
#define	MBADS_DPR	0x100		/* Unit present */

/* Definitions in mba_device md_dt */
#define	MBADT_RP04	0x10
#define MBADT_RP05	0x11
#define MBADT_RP06	0x12
#define MBADT_RP07	0x22
#define MBADT_RM02	0x15
#define MBADT_RM03	0x14
#define MBADT_RM05	0x17
#define MBADT_RM80	0x16
#define	MBADT_DRQ	0x800		/* Dual ported */
#define	MBADT_MOH	0x2000		/* Moving head device */
