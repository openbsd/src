/*	$NetBSD: ctlreg.h,v 1.8 1995/06/25 21:35:05 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)ctlreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun-4, 4c, and 4m control registers. (includes address space definitions
 * and some registers in control space).
 */

/*			0x00	   unused */
/*			0x01	   unused */
#if defined(SUN4C) || defined(SUN4)
#define	ASI_CONTROL	0x02	/* cache enable, context reg, etc */
#define	ASI_SEGMAP	0x03	/* segment maps (so we can reach each pmeg) */
#define	ASI_PTE		0x04	/* PTE space (pmegs) */
#define	ASI_REGMAP	0x06	/* region maps (3 level MMUs only) */
#define	ASI_HWFLUSHSEG	0x05	/* hardware assisted version of FLUSHSEG */
#define	ASI_HWFLUSHPG	0x06	/* hardware assisted version of FLUSHPG */
#define	ASI_HWFLUSHCTX	0x07	/* hardware assisted version of FLUSHCTX */
#endif
#if defined(SUN4M)
#define ASI_SRMMUFP	0x03	/* ref mmu flush/probe */
#define ASI_SRMMUFP_L3	(0<<8)	/* probe L3	| flush L3 PTE */
#define ASI_SRMMUFP_L2	(1<<8)	/* probe L2	| flush L2/L3 PTE/PTD's */
#define ASI_SRMMUFP_L1	(2<<8)	/* probe L1	| flush L1/L2/L3 PTE/PTD's*/
#define ASI_SRMMUFP_L0	(3<<8)	/* probe L0	| flush L0/L1/L2/L3 PTE/PTD's */
#define ASI_SRMMUFP_LN	(4<<8)	/* probe all	| flush all levels */

#define ASI_SRMMU	0x04	/* ref mmu registers */
#define ASI_SRMMUDIAG	0x06
#endif

#define	ASI_USERI	0x08	/* I-space (user) */
#define	ASI_KERNELI	0x09	/* I-space (kernel) */
#define	ASI_USERD	0x0a	/* D-space (user) */
#define	ASI_KERNELD	0x0b	/* D-space (kernel) */

#if defined(SUN4C) || defined(SUN4)
#define	ASI_FLUSHREG	0x7	/* causes hardware to flush cache region */
#define	ASI_FLUSHSEG	0x0c	/* causes hardware to flush cache segment */
#define	ASI_FLUSHPG	0x0d	/* causes hardware to flush cache page */
#define	ASI_FLUSHCTX	0x0e	/* causes hardware to flush cache context */
#endif
#if defined(SUN4)
#define	ASI_DCACHE	0x0f	/* flush data cache; not used on 4c */
#endif

#if defined(SUN4M)
#define ASI_ICACHETAG	0x0c	/* instruction cache tag */
#define ASI_ICACHEDATA	0x0d	/* instruction cache data */
#define ASI_DCACHETAG	0x0e	/* data cache tag */
#define ASI_DCACHEDATA	0x0f	/* data cache data */
#define ASI_IDCACHELFP	0x10	/* ms2 only: flush i&d cache line (page) */
#define ASI_IDCACHELFS	0x11	/* ms2 only: flush i&d cache line (seg) */
#define ASI_IDCACHELFR	0x12	/* ms2 only: flush i&d cache line (reg) */
#define ASI_IDCACHELFC	0x13	/* ms2 only: flush i&d cache line (ctxt) */
#define ASI_IDCACHELFU	0x14	/* ms2 only: flush i&d cache line (user) */
#define ASI_BYPASS	0x20	/* sun ref mmu bypass, ie. direct phys access */
#define ASI_ICACHECLR	0x36	/* ms1 only: instruction cache flash clear */
#define ASI_DCACHECLR	0x37	/* ms1 only: data cache clear */
#define ASI_DCACHEDIAG	0x39	/* data cache diagnostic register access */
#endif

#if defined(SUN4C) || defined(SUN4)
/* registers in the control space */
#define	AC_CONTEXT	0x30000000	/* context register (byte) */
#define	AC_SYSENABLE	0x40000000	/* system enable register (byte) */
#define	AC_CACHETAGS	0x80000000	/* cache tag base address */
#define	AC_SERIAL	0xf0000000	/* special serial port sneakiness */
	/* AC_SERIAL is not used in the kernel (it is for the PROM) */
#endif

#if defined(SUN4)
#define	AC_IDPROM	0x00000000	/* ID PROM */
#define	AC_DVMA_ENABLE	0x50000000	/* enable user dvma */
#define	AC_BUS_ERR	0x60000000	/* bus error register */
#define	AC_DIAG_REG	0x70000000	/* diagnostic reg */
#define	AC_DVMA_MAP	0xd0000000	/* user dvma map entries */
#define AC_VMEINTVEC	0xe0000000	/* vme interrupt vector */

/* XXX: does not belong here */
#define	ME_REG_IERR	0x80		/* memory err ctrl reg error intr pending bit */
#endif

#if defined(SUN4C)
#define	AC_SYNC_ERR	0x60000000	/* sync (memory) error reg */
#define	AC_SYNC_VA	0x60000004	/* sync error virtual addr */
#define	AC_ASYNC_ERR	0x60000008	/* async error reg */
#define	AC_ASYNC_VA	0x6000000c	/* async error virtual addr */
#define	AC_CACHEDATA	0x90000000	/* cached data */
#endif

#if defined(SUN4C) || defined(SUN4)
/*
 * Bits in sync error register.  Reading the register clears these;
 * otherwise they accumulate.  The error(s) occurred at the virtual
 * address stored in the sync error address register, and may have
 * been due to, e.g., what would usually be called a page fault.
 * Worse, the bits accumulate during instruction prefetch, so
 * various bits can be on that should be off.
 */
#define	SER_WRITE	0x8000		/* error occurred during write */
#define	SER_INVAL	0x80		/* PTE had PG_V off */
#define	SER_PROT	0x40		/* operation violated PTE prot */
#define	SER_TIMEOUT	0x20		/* bus timeout (non-existent mem) */
#define	SER_SBUSERR	0x10		/* S-Bus bus error */
#define	SER_MEMERR	0x08		/* memory ecc/parity error */
#define	SER_SZERR	0x02		/* size error, whatever that is */
#define	SER_WATCHDOG	0x01		/* watchdog reset (never see this) */

#define	SER_BITS \
"\20\20WRITE\10INVAL\7PROT\6TIMEOUT\5SBUSERR\4MEMERR\2SZERR\1WATCHDOG"

/*
 * Bits in async error register (errors from DVMA or Sun-4 cache
 * writeback).  The corresponding bit is also set in the sync error reg.
 *
 * A writeback invalid error means there is a bug in the PTE manager.
 *
 * The word is that the async error register does not work right.
 */
#define	AER_WBINVAL	0x80		/* writeback found PTE without PG_V */
#define	AER_TIMEOUT	0x20		/* bus timeout */
#define	AER_DVMAERR	0x10		/* bus error during DVMA */

#define	AER_BITS	"\20\10WBINVAL\6TIMEOUT\5DVMAERR"

/*
 * Bits in system enable register.
 */
#define	SYSEN_DVMA	0x20		/* Enable dvma */
#define	SYSEN_CACHE	0x10		/* Enable cache */
#define	SYSEN_IOCACHE	0x40		/* Enable IO cache */
#define	SYSEN_VIDEO	0x08		/* Enable on-board video */
#define	SYSEN_RESET	0x04		/* Reset the hardware */
#define	SYSEN_RESETVME	0x02		/* Reset the VME bus */
#endif

#if defined(SUN4M)
#define SRMMU_PCR	0x00000000	/* Processor control register */
#define SRMMU_CXTPTR	0x00000100	/* Context table pointer register */
#define SRMMU_CXR	0x00000200	/* Context register */
#define SRMMU_SFSTAT	0x00000300	/* Synchronous fault status reg */
#define SRMMU_SFADDR	0x00000400	/* Synchronous fault address reg */
#define SRMMU_TLBCTRL	0x00001000	/* TLB replacement control reg */

/* Synchronous Fault Status Register bits */
#define SFSR_CS		0x00010000	/* Control Space error */
#define SFSR_PERR	0x00006000	/* Parity error code */
#define SFSR_TO		0x00000800	/* S-Bus timeout */
#define SFSR_BE		0x00000400	/* S-Bus bus error */
#define SFSR_LVL	0x00000300	/* Pagetable level causing the fault */
#define SFSR_AT		0x000000e0	/* Access type */
#define SFSR_FT		0x0000001c	/* Fault type */
#define SFSR_FAV	0x00000002	/* Fault Address is valid */
#define SFSR_OW		0x00000001	/* Overwritten with new fault */

/* TLB Replacement Control Register bits */
#define TLBC_DISABLE	0x00000020	/* Disable replacement counter */
#define TLBC_RCNTMASK	0x0000001f	/* Replacement counter (0-31) */
#endif
