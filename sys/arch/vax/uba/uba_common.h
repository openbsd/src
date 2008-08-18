/*	$OpenBSD: uba_common.h,v 1.5 2008/08/18 23:10:39 miod Exp $	*/
/*	$NetBSD: uba_common.h,v 1.2 1999/06/21 16:23:01 ragge Exp $ */
/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)ubareg.h	7.8 (Berkeley) 5/9/91
 */

/*
 * VAX-specific parts of the Unibus softc.
 */
struct uba_regs;

struct	uba_vsoftc {
	struct	uba_softc uv_sc;/* Common vars from arch/vax/qbus/ubavar.h */
	struct	vax_bus_dma_tag uv_dmat;
	struct	vax_sgmap uv_sgmap;
	int	uv_size;	/* Size of UBA addressable memory */
	paddr_t uv_addr;	/* Physical address of map registers */
	struct	uba_regs *uv_uba;/* Where applicable */
	int	uh_ibase;
	int	uh_zvtime;
	int	uh_zvtotal;
	int	uh_zvcnt;
};

/*
 * Size of unibus memory address space in pages
 * (also number of map registers).
 */
#define UBAPAGES	496
#define UBAIOADDR	0760000	 /* start of I/O page */
#define UBAIOPAGES	16

/*
 * DW780/DW750 hardware registers
 */
struct uba_regs {
	int	uba_cnfgr;		/* configuration register */
	int	uba_cr;			/* control register */
	int	uba_sr;			/* status register */
	int	uba_dcr;		/* diagnostic control register */
	int	uba_fmer;		/* failed map entry register */
	int	uba_fubar;		/* failed UNIBUS address register */
	int	pad1[2];
	int	uba_brsvr[4];
	int	uba_brrvr[4];		/* receive vector registers */
	int	uba_dpr[16];		/* buffered data path register */
	int	pad2[480];
	pt_entry_t uba_map[UBAPAGES];	/* unibus map register */
	int	pad3[UBAIOPAGES];	/* no maps for device address space */
};
#define	UBA_REGS_DEFINED

void	uba_dma_init(struct uba_vsoftc *);
