/*	$OpenBSD: ka650.h,v 1.11 2008/08/18 23:07:24 miod Exp $	*/
/*	$NetBSD: ka650.h,v 1.6 1997/07/26 10:12:43 ragge Exp $	*/
/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mt. Xinu.
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
 *	@(#)ka650.h	7.5 (Berkeley) 6/28/90
 */

/*
 *
 * Definitions specific to the ka650 (uVAX 3600/3602) cpu card.
 */

/*
 * CAER: Memory System Error Register (IPR 39)
 */
#define CAER_DAL	0x00000040	/* CDAL or level 2 cache data parity */
#define CAER_MCD	0x00000020	/* mcheck due to DAL parity error */
#define CAER_MCC	0x00000010	/* mcheck due to 1st lev cache parity */
#define CAER_DAT	0x00000002	/* data parity in 1st level cache */
#define CAER_TAG	0x00000001	/* tag parity in 1st level cache */

/*
 * DMA System Error Register (merr_dser)
 */
#define DSER_QNXM	0x00000080	/* Q-22 Bus NXM */
#define DSER_QPE	0x00000020	/* Q-22 Bus parity Error */
#define DSER_MEM	0x00000010	/* Main mem err due to ext dev DMA */
#define DSER_LOST	0x00000008	/* Lost error: DSER <7,5,4,0> set */
#define DSER_NOGRANT	0x00000004	/* No Grant timeout on cpu demand R/W */
#define DSER_DNXM	0x00000001	/* DMA NXM */
#define DSER_CLEAR 	(DSER_QNXM | DSER_QPE | DSER_MEM |  \
			 DSER_LOST | DSER_NOGRANT | DSER_DNXM)
#define DMASER_BITS \
"\20\20BHALT\17DCNEG\10QBNXM\6QBPE\5MEMERR\4LOSTERR\3NOGRANT\1DMANXM"

#ifndef _LOCORE
/*
 * Local registers (in I/O space)
 * This is done in disjoint sections.  Map names are set in locore.s
 * and they are mapped in routine configcpu()
 */

/*
 * memory error & configuration registers
 */
struct ka650_merr {
	u_long	merr_scr;	/* System Config Register */
	u_long	merr_dser;	/* DMA System Error Register */
	u_long	merr_qbear;	/* QBus Error Address Register */
	u_long	merr_dear;	/* DMA Error Address Register */
	u_long	merr_qbmbr;	/* Q Bus Map Base address Register */
	u_long	pad[59];
	u_long	merr_csr[16];	/* Main Memory Config Regs (16 banks) */
	u_long	merr_errstat;	/* Main Memory Error Status */
	u_long	merr_cont;	/* Main Memory Control */
};
#define KA650_MERR	0x20080000

/*
 * Main Memory Error Status Register (merr_errstat)
 */
#define MEM_EMASK	0xe0000180	/* mask of all err bits */
#define MEM_RDS		0x80000000	/* uncorrectable main memory */
#define MEM_RDSHIGH	0x40000000	/* high rate RDS errors */
#define MEM_CRD		0x20000000	/* correctable main memory */
#define MEM_DMA		0x00000100	/* DMA read or write error */
#define MEM_CDAL	0x00000080	/* CDAL Parity error on write */
#define MEM_PAGE	0x1ffffe00	/* Offending Page Number */
#define MEM_PAGESHFT	9		/* Shift to normalize page number */

/*
 * Main Memory Control & Diag Status Reg (merr_cont)
 */
#define MEM_CRDINT	0x00001000	/* CRD interrupts enabled */
#define MEM_REFRESH	0x00000800	/* Forced memory refresh */
#define MEM_ERRDIS	0x00000400	/* error detect disable	*/
#define MEM_DIAG	0x00000080	/* Diagnostics mode */
#define MEM_CHECK	0x0000007f	/* check bits for diagnostic mode */

/*
 * Main Memory Config Regs (merr_csr[0-15])
 */
#define MEM_BNKENBLE	0x80000000	/* Bank Enable */
#define MEM_BNKNUM	0x03c00000	/* Physical map Bank number */
#define MEM_BNKUSAGE	0x00000003	/* Bank Usage */

/*
 * Cache Control & Boot/Diag registers
 */
struct ka650_cbd {
	u_char	cbd_cacr;	/* Low byte: Cache Enable & Parity Err detect */
	u_char	cbd_cdf1;	/* Cache diagnostic field (unused) */
	u_char	cbd_cdf2;	/* Cache diagnostic field (unused) */
	u_char	pad;
	u_long	cbd_bdr;	/* Boot & Diagnostic Register (unused) */
};
#define KA650_CBD	0x20084000

/*
 * CACR: Cache Control Register (2nd level cache) (cbd_cacr)
 */
#define CACR_CEN	0x00000010	/* Cache enable */
#define CACR_CPE	0x00000020	/* Cache Parity Error */

/*
 * Inter Processor Communication Register
 * To determine if memory error was from QBUS device DMA (as opposed to cpu).
 */
struct ka650_ipcr {
	u_long	pad[80];
	u_short	ipcr0;		/* InterProcessor Comm Reg for arbiter */
};
#define KA650_IPCR	0x20001e00

#endif /* _LOCORE */

/*
 * Physical start address of the Qbus memory.
 * The q-bus memory size is 4 meg.
 * Physical start address of the I/O space (where the 8Kbyte I/O page is).
 */
#define KA650_QMEM	0x30000000
#define KA650_QMEMSIZE	(512*8192)
#define KA650_QDEVADDR	0x20000000

/*
 * Mapping info for Cache Entries, including
 * Size (in bytes) of 2nd Level Cache for cache flush operation
 */
#define KA650_CACHE	0x10000000
#define KA650_CACHESIZE	(64*1024)

/*
 * Useful ROM addresses
 */
#define	KA650ROM_SIDEX	0x20060004	/* system ID extension */
#define	KA650ROM_GETC	0x20060008	/* (jsb) get character from console */
#define	KA650ROM_PUTS	0x2006000c	/* (jsb) put string to console */
#define	KA650ROM_GETS	0x20060010	/* (jsb) read string with prompt */
#define KA650_CONSTYPE	0x20140401	/* byte at which console type resides */

/*
 * Some useful macros
 */
#define	GETCPUTYPE(x)	((x >> 24) & 0xff)
#define	GETSYSSUBT(x)	((x >> 8) & 0xff)
#define	GETFRMREV(x)	((x >> 16) & 0xff)
#define	GETCODREV(x)	(x & 0xff)
