/*	$OpenBSD: rpb.h,v 1.11 2008/08/18 23:19:24 miod Exp $ */
/*	$NetBSD: rpb.h,v 1.6 1998/07/01 09:37:11 ragge Exp $ */
/*
 * Copyright (c) 1995 Ludd, University of Lule}, Sweden.
 * All rights reserved.
 *
 * This code is derived from software contributed to Ludd by Bertram Barth.
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
 *      This product includes software developed at Ludd, University of 
 *      Lule}, Sweden and its contributors.
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

/*
 * Look at "VAX/VMS Internals and Data Structures" around page 907
 * to get more info about RPB.
 */

struct rpb {		/* size		description */
	struct rpb *rpb_base;	/* 4  physical base address of block */
	void	(*rpb_restart)(void);/* 4  physical address of restart routine */
	long	rpb_chksum;/* 4  checksum of first 31 longwords of restart */
	long	rpb_rstflg;	/* 4  Restart in progress flag */
	long	rpb_haltpc;	/* 4  PC at HALT/restart */
			/* offset: 20 */
	long	rpb_haltpsl;/* 4  PSL at HALT/restart */
	long	rpb_haltcode;/* 4  reason for restart */
	long	rpb_bootr0;/* 24  Saved bootstrap parameters (R0 through R5) */
	long	rpb_bootr1;
	long	rpb_bootr2;
	long	rpb_bootr3;
	long	rpb_bootr4;
	long	rpb_bootr5;
	long	iovec;	/* 4  Address of bootstrap driver */
	long	iovecsz;/* 4  Size (in bytes) of bootstrap driver */
			/* offset: 60 */
	long	fillbn;	/* 4  LBN of seconday bootstrap file */
	long	filsiz;	/* 4  Size (in blocks) of seconday bootstrap file */
	long	pfnmap[2];	/* 8  Descriptor of PFN bitmap */
	long	pfncnt;	/* 4  Count of physical pages */
			/* offset: 80 */
	long	svaspt;	/* 4  system virtual address of system page table */
	long	csrphy;	/* 4  Physical Address of UBA device CSR */
	long	csrvir;	/* 4  Virtual Address of UBA device CSR */
	long	adpphy;	/* 4  Physical Address of adapter configurate reg. */
	long	adpvir;	/* 4  Virtual Address of adapter configurate reg. */
			/* offset: 100 */
	short	unit;	/* 2  Bootstrap device unit number */
	u_char	devtyp;	/* 1  Bootstrap device type code */
	u_char	slave;	/* 1  Bootstrap device slave unit number */
	char	file[40];	/* 40  Secondary bootstrap file name */
	u_char	confreg[16];	/* 16  Byte array of adapter types */
			/* offset: 160 */
#if 0
	u_char	hdrpgcnt; /* 1  Count of header pages in 2nd bootstrap image */
	short	bootndt;/* 2  Type of boot adapter */
	u_char	flags;	/* 1  Miscellaneous flag bits */
#else
	long	align;	/* if the compiler doesnt proper alignment */
#endif
	long	max_pfn;/* 4  Absolute highest PFN */
	long	sptep;	/* 4  System space PTE prototype register */
	long	sbr;	/* 4  Saved system base register */
	long	cpudbvec;/* 4  Physical address of per-CPU database vector */
			/* offset: 180 */
	long	cca_addr;	/* 4  Physical address of CCA */
	long	slr;	/* 4  Saved system length register */
	long	memdesc[16];	/* 64  Longword array of memory descriptors */
	long	smp_pc;	/* 4  SMP boot page physical address */
	long	wait;	/* 4  Bugcheck loop code for attached processor */
			/* offset: 260 */
	long	badpgs;	/* 4  Number of bad pages found in memory scan */
	u_char	ctrlltr;/* 1  Controller letter designation */
	u_char	scbpagct;	/* 1  SCB page count */
	u_char	reserved[6];	/* 6  -- */
	long	vmb_revision;	/* 4  VMB revision label */
};

/*
 * Bootstrap device number encoding.
 */
#define	BDEV_HP		0
#define	BDEV_RK		1
#define	BDEV_RL		2
#define	BDEV_IDC	3
#define	BDEV_UDA	17
#define	BDEV_TK		18
#define	BDEV_HSC	32
#define	BDEV_KDB	33
#define	BDEV_KRB	34
#define	BDEV_NK		35
#define	BDEV_RD		36	/* ST506/MFM disk on HDC9224 */
#define	BDEV_ST		37	/* SCSI tape on NCR5380 */
#define	BDEV_SDS	39	/* SCSI disk on SII */
#define	BDEV_SD		42	/* SCSI disk on NCR5380 */
#define BDEV_SDN	46	/* SCSI disk on NCR5394 */
#define	BDEV_CNSL	64
#define	BDEV_QE		96
#define	BDEV_DE		97
#define	BDEV_NI		98
#define	BDEV_LE		99
#define	BDEV_ZE		100

#define	BDEV_NET 	BDEV_QE		/* first network BDEV */

#ifdef _KERNEL
extern struct rpb rpb;
#endif
