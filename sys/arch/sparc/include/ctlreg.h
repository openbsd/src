/*	$OpenBSD: ctlreg.h,v 1.7 2003/06/02 23:27:54 millert Exp $	*/
/*	$NetBSD: ctlreg.h,v 1.15 1997/07/20 18:55:03 pk Exp $ */

/*
 * Copyright (c) 1996
 *	The President and Fellows of Harvard College. All rights reserved.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by Harvard University.
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
 *	@(#)ctlreg.h	8.1 (Berkeley) 6/11/93
 */

/*
 * Sun4m support by Aaron Brown, Harvard University.
 * Changes Copyright (c) 1995 The President and Fellows of Harvard College.
 * All rights reserved.
 */

/*
 * Sun 4, 4c, and 4m control registers. (includes address space definitions
 * and some registers in control space).
 */

/*
 * The Alternate address spaces.
 */

/*			0x00	   unused */
/*			0x01	   unused */
#define	ASI_CONTROL	0x02	/* cache enable, context reg, etc */
#define	ASI_SEGMAP	0x03	/* [4/4c] segment maps */
#define ASI_SRMMUFP	0x03	/* [4m] ref mmu flush/probe */
#define	ASI_PTE		0x04	/* [4/4c] PTE space (pmegs) */
#define ASI_SRMMU	0x04	/* [4m] ref mmu registers */
#define	ASI_REGMAP	0x06	/* [4/3-level MMU ] region maps */
#define	ASI_HWFLUSHSEG	0x05	/* [4/4c] hardware assisted version of FLUSHSEG */
#define	ASI_HWFLUSHPG	0x06	/* [4/4c] hardware assisted version of FLUSHPG */
#define ASI_SRMMUDIAG	0x06	/* [4m] */
#define	ASI_HWFLUSHCTX	0x07	/* [4/4c] hardware assisted version of FLUSHCTX */

#define	ASI_USERI	0x08	/* I-space (user) */
#define	ASI_KERNELI	0x09	/* I-space (kernel) */
#define	ASI_USERD	0x0a	/* D-space (user) */
#define	ASI_KERNELD	0x0b	/* D-space (kernel) */

#define	ASI_FLUSHREG	0x7	/* [4/4c] flush cache by region */
#define	ASI_FLUSHSEG	0x0c	/* [4/4c] flush cache by segment */
#define	ASI_FLUSHPG	0x0d	/* [4/4c] flush cache by page */
#define	ASI_FLUSHCTX	0x0e	/* [4/4c] flush cache by context */

#define	ASI_DCACHE	0x0f	/* [4] flush data cache */

#define ASI_ICACHETAG	0x0c	/* [4m] instruction cache tag */
#define ASI_ICACHEDATA	0x0d	/* [4m] instruction cache data */
#define ASI_DCACHETAG	0x0e	/* [4m] data cache tag */
#define ASI_DCACHEDATA	0x0f	/* [4m] data cache data */
#define ASI_IDCACHELFP	0x10	/* [4m] flush i&d cache line (page) */
#define ASI_IDCACHELFS	0x11	/* [4m] flush i&d cache line (seg) */
#define ASI_IDCACHELFR	0x12	/* [4m] flush i&d cache line (reg) */
#define ASI_IDCACHELFC	0x13	/* [4m] flush i&d cache line (ctxt) */
#define ASI_IDCACHELFU	0x14	/* [4m] flush i&d cache line (user) */
#define ASI_BYPASS	0x20	/* [4m] sun ref mmu bypass,
				        ie. direct phys access */
#define ASI_HICACHECLR	0x31	/* [4m] hypersparc only: I-cache flash clear */
#define ASI_ICACHECLR	0x36	/* [4m] ms1 only: I-cache flash clear */
#define ASI_DCACHECLR	0x37	/* [4m] ms1 only: D-cache flash clear */
#define ASI_DCACHEDIAG	0x39	/* [4m] data cache diagnostic register access */

/*
 * [4/4c] Registers in the control space (ASI_CONTROL).
 */
#define	AC_IDPROM	0x00000000	/* [4] ID PROM */
#define	AC_CONTEXT	0x30000000	/* [4/4c] context register (byte) */
#define	AC_SYSENABLE	0x40000000	/* [4/4c] system enable register (byte) */
#define	AC_DVMA_ENABLE	0x50000000	/* [4] enable user dvma */
#define	AC_BUS_ERR	0x60000000	/* [4] bus error register */
#define	AC_SYNC_ERR	0x60000000	/* [4c] sync (memory) error reg */
#define	AC_SYNC_VA	0x60000004	/* [4c] sync error virtual addr */
#define	AC_ASYNC_ERR	0x60000008	/* [4c] async error reg */
#define	AC_ASYNC_VA	0x6000000c	/* [4c] async error virtual addr */
#define	AC_DIAG_REG	0x70000000	/* [4] diagnostic reg */
#define	AC_CACHETAGS	0x80000000	/* [4/4c?] cache tag base address */
#define	AC_CACHEDATA	0x90000000	/* [4] cached data [sun4/400?] */
#define	AC_DVMA_MAP	0xd0000000	/* [4] user dvma map entries */
#define AC_VMEINTVEC	0xe0000000	/* [4] vme interrupt vector */
#define	AC_SERIAL	0xf0000000	/* [4/4c] special serial port sneakiness */
	/* AC_SERIAL is not used in the kernel (it is for the PROM) */

/* XXX: does not belong here */
#define	ME_REG_IERR	0x80		/* memory err ctrl reg error intr pending bit */

/*
 * [4/4c]
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
#define	SER_SZERR	0x02		/* [4/vme?] size error, whatever that is */
#define	SER_WATCHDOG	0x01		/* watchdog reset (never see this) */

#define	SER_BITS \
"\20\20WRITE\10INVAL\7PROT\6TIMEOUT\5SBUSERR\4MEMERR\2SZERR\1WATCHDOG"

/*
 * [4/4c]
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
 * [4/4c] Bits in system enable register.
 */
#define	SYSEN_DVMA	0x20		/* Enable dvma */
#define	SYSEN_CACHE	0x10		/* Enable cache */
#define	SYSEN_IOCACHE	0x40		/* Enable IO cache */
#define	SYSEN_VIDEO	0x08		/* Enable on-board video */
#define	SYSEN_RESET	0x04		/* Reset the hardware */
#define	SYSEN_RESETVME	0x02		/* Reset the VME bus */


/*
 * [4m] Bits in ASI_CONTROL? space, sun4m only.
 */
#define MXCC_ENABLE_ADDR	0x1c00a00	/* Enable register for MXCC */
#define MXCC_ENABLE_BIT		0x4		/* Enable bit for MXCC */

/*
 * Bits in ASI_SRMMUFP space.
 *	Bits 8-11 determine the type of flush/probe.
 *	Address bits 12-31 hold the page frame.
 */
#define ASI_SRMMUFP_L3	(0<<8)	/* probe L3	| flush L3 PTE */
#define ASI_SRMMUFP_L2	(1<<8)	/* probe L2	| flush L2/L3 PTE/PTD's */
#define ASI_SRMMUFP_L1	(2<<8)	/* probe L1	| flush L1/L2/L3 PTE/PTD's*/
#define ASI_SRMMUFP_L0	(3<<8)	/* probe L0	| flush L0/L1/L2/L3 PTE/PTD's */
#define ASI_SRMMUFP_LN	(4<<8)	/* probe all	| flush all levels */

/*
 * [4m] Registers and bits in the SPARC Reference MMU (ASI_SRMMU).
 */
#define SRMMU_PCR	0x00000000	/* Processor control register */
#define SRMMU_CXTPTR	0x00000100	/* Context table pointer register */
#define SRMMU_CXR	0x00000200	/* Context register */
#define SRMMU_SFSR	0x00000300	/* Synchronous fault status reg */
#define SRMMU_SFAR	0x00000400	/* Synchronous fault address reg */
#define SRMMU_AFSR	0x00000500	/* Asynchronous fault status reg (HS)*/
#define SRMMU_AFAR	0x00000600	/* Asynchronous fault address reg (HS)*/
#define SRMMU_PCFG	0x00000600	/* Processor configuration reg (TURBO)*/
#define SRMMU_TLBCTRL	0x00001000	/* TLB replacement control reg */


/*
 * [4m] Bits in SRMMU control register. One set per module.
 */
#define VIKING_PCR_ME	0x00000001	/* MMU Enable */
#define VIKING_PCR_NF	0x00000002	/* Fault inhibit bit */
#define VIKING_PCR_PSO	0x00000080	/* Partial Store Ordering enable */
#define VIKING_PCR_DCE	0x00000100	/* Data cache enable bit */
#define VIKING_PCR_ICE	0x00000200	/* SuperSPARC instr. cache enable */
#define VIKING_PCR_SB	0x00000400	/* Store buffer enable bit */
#define VIKING_PCR_MB	0x00000800	/* MBus mode: 0=MXCC, 1=no MXCC */
#define VIKING_PCR_PE	0x00001000	/* Enable memory parity checking */
#define VIKING_PCR_BM	0x00002000	/* 1 iff booting */
#define VIKING_PCR_SE	0x00004000	/* Coherent bus snoop enable */
#define VIKING_PCR_AC	0x00008000	/* 1=cache non-MMU accesses */
#define	VIKING_PCR_TC	0x00010000	/* 1=cache table walks */

#define HYPERSPARC_PCR_ME	0x00000001	/* MMU Enable */
#define HYPERSPARC_PCR_NF	0x00000002	/* Fault inhibit bit */
#define HYPERSPARC_PCR_CE	0x00000100	/* Cache enable bit */
#define HYPERSPARC_PCR_CM	0x00000400	/* Cache mode: 1=write-back */
#define	HYPERSPARC_PCR_MR	0x00000800	/* Memory reflection: 1 = on */
#define HYPERSPARC_PCR_CS	0x00001000	/* cache size: 1=256k, 0=128k */
#define HYPERSPARC_PCR_C	0x00002000	/* enable cache when MMU off */
#define HYPERSPARC_PCR_BM	0x00004000	/* 1 iff booting */
#define HYPERSPARC_PCR_MID	0x00078000	/* MBus module ID MID<3:0> */
#define HYPERSPARC_PCR_WBE	0x00080000	/* Write buffer enable */
#define HYPERSPARC_PCR_SE	0x00100000	/* Coherent bus snoop enable */
#define HYPERSPARC_PCR_CWR	0x00200000	/* Cache wrap enable */

#define CYPRESS_PCR_ME	0x00000001	/* MMU Enable */
#define CYPRESS_PCR_NF	0x00000002	/* Fault inhibit bit */
#define CYPRESS_PCR_CE	0x00000100	/* Cache enable bit */
#define CYPRESS_PCR_CL	0x00000200	/* Cache Lock (604 only) */
#define CYPRESS_PCR_CM	0x00000400	/* Cache mode: 1=write-back */
#define	CYPRESS_PCR_MR	0x00000800	/* Memory reflection: 1=on (605 only) */
#define CYPRESS_PCR_C	0x00002000	/* enable cache when MMU off */
#define CYPRESS_PCR_BM	0x00004000	/* 1 iff booting */
#define CYPRESS_PCR_MID	0x00078000	/* MBus module ID MID<3:0> (605 only) */
#define CYPRESS_PCR_MV	0x00080000	/* Multichip Valid */
#define CYPRESS_PCR_MCM	0x00300000	/* Multichip Mask */
#define CYPRESS_PCR_MCA	0x00c00000	/* Multichip Address */

#define MS1_PCR_ME	0x00000001	/* MMU Enable */
#define MS1_PCR_NF	0x00000002	/* Fault inhibit bit */
#define MS1_PCR_DCE	0x00000100	/* Data cache enable */
#define MS1_PCR_ICE	0x00000200	/* Instruction cache enable */
#define MS1_PCR_RC	0x00000c00	/* DRAM Refresh control */
#define MS1_PCR_PE	0x00001000	/* Enable memory parity checking */
#define MS1_PCR_BM	0x00004000	/* 1 iff booting */
#define MS1_PCR_AC	0x00008000	/* 1=cache if ME==0 (and [ID]CE on) */
#define	MS1_PCR_ID	0x00010000	/* 1=disable ITBR */
#define	MS1_PCR_PC	0x00020000	/* Parity control: 0=even,1=odd */
#define	MS1_PCR_MV	0x00100000	/* Memory data View (diag) */
#define	MS1_PCR_DV	0x00200000	/* Data View (diag) */
#define	MS1_PCR_AV	0x00400000	/* Address View (diag) */
#define	MS1_PCR_STW	0x00800000	/* Software Tablewalk enable */

#define SWIFT_PCR_ME	0x00000001	/* MMU Enable */
#define SWIFT_PCR_NF	0x00000002	/* Fault inhibit bit */
#define SWIFT_PCR_DCE	0x00000100	/* Data cache enable */
#define SWIFT_PCR_ICE	0x00000200	/* Instruction cache enable */
#define SWIFT_PCR_RC	0x00003c00	/* DRAM Refresh control */
#define SWIFT_PCR_BM	0x00004000	/* 1 iff booting */
#define SWIFT_PCR_AC	0x00008000	/* 1=cache if ME=0 (and [ID]CE on) */
#define	SWIFT_PCR_PA	0x00010000	/* TCX/SX control */
#define	SWIFT_PCR_PC	0x00020000	/* Parity control: 0=even,1=odd */
#define SWIFT_PCR_PE	0x00040000	/* Enable memory parity checking */
#define	SWIFT_PCR_PMC	0x00180000	/* Page mode control */
#define	SWIFT_PCR_BF	0x00200000	/* Branch Folding */
#define	SWIFT_PCR_WP	0x00400000	/* Watch point enable */
#define	SWIFT_PCR_STW	0x00800000	/* Software Tablewalk enable */

#define TURBOSPARC_PCR_ME	0x00000001	/* MMU Enable */
#define TURBOSPARC_PCR_NF	0x00000002	/* Fault inhibit bit */
#define TURBOSPARC_PCR_ICS	0x00000004	/* I-cache snoop enable */
#define TURBOSPARC_PCR_PSO	0x00000008	/* Partial Store order (ro!) */
#define TURBOSPARC_PCR_DCE	0x00000100	/* Data cache enable */
#define TURBOSPARC_PCR_ICE	0x00000200	/* Instruction cache enable */
#define TURBOSPARC_PCR_RC	0x00003c00	/* DRAM Refresh control */
#define TURBOSPARC_PCR_BM	0x00004000	/* 1 iff booting */
#define	TURBOSPARC_PCR_PC	0x00020000	/* Parity ctrl: 0=even,1=odd */
#define TURBOSPARC_PCR_PE	0x00040000	/* Enable parity checking */
#define	TURBOSPARC_PCR_PMC	0x00180000	/* Page mode control */

/* The Turbosparc's Processor Configuration Register */
#define	TURBOSPARC_PCFG_SCC	0x00000007	/* e-cache config */
#define	TURBOSPARC_PCFG_SE	0x00000008	/* e-cache enable */
#define	TURBOSPARC_PCFG_US2	0x00000010	/* microsparc II compat */
#define	TURBOSPARC_PCFG_WT	0x00000020	/* write-through enable */
#define	TURBOSPARC_PCFG_SBC	0x000000c0	/* SBus Clock */
#define	TURBOSPARC_PCFG_WS	0x03800000	/* DRAM wait states */
#define	TURBOSPARC_PCFG_RAH	0x0c000000	/* DRAM Row Address Hold */
#define	TURBOSPARC_PCFG_AXC	0x30000000	/* AFX Clock */
#define	TURBOSPARC_PCFG_SNP	0x40000000	/* DVMA Snoop enable */
#define	TURBOSPARC_PCFG_IOCLK	0x80000000	/* I/O clock ratio */


/* Implementation and Version fields are common to all modules */
#define SRMMU_PCR_VER	0x0f000000	/* Version of MMU implementation */
#define SRMMU_PCR_IMPL	0xf0000000	/* Implementation number of MMU */


/* [4m] Bits in the Synchronous Fault Status Register */
#define SFSR_EM		0x00020000	/* Error mode watchdog reset occurred */
#define SFSR_CS		0x00010000	/* Control Space error */
#define SFSR_PERR	0x00006000	/* Parity error code */
#define SFSR_SB		0x00008000	/* SS: Store Buffer Error */
#define SFSR_P		0x00004000	/* SS: Parity error */
#define SFSR_UC		0x00001000	/* Uncorrectable error */
#define SFSR_TO		0x00000800	/* S-Bus timeout */
#define SFSR_BE		0x00000400	/* S-Bus bus error */
#define SFSR_LVL	0x00000300	/* Pagetable level causing the fault */
#define SFSR_AT		0x000000e0	/* Access type */
#define SFSR_FT		0x0000001c	/* Fault type */
#define SFSR_FAV	0x00000002	/* Fault Address is valid */
#define SFSR_OW		0x00000001	/* Overwritten with new fault */

#define	SFSR_BITS \
"\20\21CSERR\17PARITY\16SYSERR\15UNCORR\14TIMEOUT\13BUSERR\2FAV\1OW"

/* [4m] Synchronous Fault Types */
#define SFSR_FT_NONE		(0 << 2) 	/* no fault */
#define SFSR_FT_INVADDR		(1 << 2)	/* invalid address fault */
#define SFSR_FT_PROTERR		(2 << 2)	/* protection fault */
#define SFSR_FT_PRIVERR		(3 << 2)	/* privelege violation */
#define SFSR_FT_TRANSERR	(4 << 2)	/* translation fault */
#define SFSR_FT_BUSERR		(5 << 2)	/* access bus error */
#define SFSR_FT_INTERR		(6 << 2)	/* internal error */
#define SFSR_FT_RESERVED	(7 << 2)	/* reserved */

/* [4m] Synchronous Fault Access Types */
#define SFSR_AT_LDUDATA		(0 << 5)     	/* Load user data */
#define SFSR_AT_LDSDATA		(1 << 5)	/* Load supervisor data */
#define SFSR_AT_LDUTEXT		(2 << 5)	/* Load user text */
#define SFSR_AT_LDSTEXT		(3 << 5)	/* Load supervisor text */
#define SFSR_AT_STUDATA		(4 << 5)	/* Store user data */
#define SFSR_AT_STSDATA		(5 << 5) 	/* Store supervisor data */
#define SFSR_AT_STUTEXT		(6 << 5)	/* Store user text */
#define SFSR_AT_STSTEXT		(7 << 5)	/* Store supervisor text */
#define SFSR_AT_SUPERVISOR	(1 << 5)	/* Set iff supervisor */
#define SFSR_AT_TEXT		(2 << 5)	/* Set iff text */
#define SFSR_AT_STORE		(4 << 5)	/* Set iff store */

/* [4m] Synchronous Fault PT Levels */
#define SFSR_LVL_0		(0 << 8)	/* Context table entry */
#define SFSR_LVL_1		(1 << 8)	/* Region table entry */
#define SFSR_LVL_2		(2 << 8)	/* Segment table entry */
#define SFSR_LVL_3		(3 << 8)	/* Page table entry */

/* [4m] Asynchronous Fault Status Register bits */
#define AFSR_AFO	0x00000001	/* Async. fault occurred */
#define AFSR_AFA	0x000000f0	/* Bits <35:32> of faulting phys addr */
#define AFSR_AFA_RSHIFT	4		/* Shift to get AFA to bit 0 */
#define AFSR_AFA_LSHIFT	28		/* Shift to get AFA to bit 32 */
#define AFSR_BE		0x00000400	/* Bus error */
#define AFSR_TO		0x00000800	/* Bus timeout */
#define AFSR_UC		0x00001000	/* Uncorrectable error */
#define AFSR_SE		0x00002000	/* System error */

#define	AFSR_BITS	"\20\16SYSERR\15UNCORR\14TIMEOUT\13BUSERR\1AFO"

/* [4m] TLB Replacement Control Register bits */
#define TLBC_DISABLE	0x00000020	/* Disable replacement counter */
#define TLBC_RCNTMASK	0x0000001f	/* Replacement counter (0-31) */

/*
 * The Ross Hypersparc has an Instruction Cache Control Register (ICCR)
 * It contains an enable bit for the on-chip instruction cache and a bit
 * that controls whether a FLUSH instruction causes an Unimplemented
 * Flush Trap or just flushes the appropriate instruction cache line.
 * The ICCR register is implemented as Ancillary State register number 31.
 */
#define	HYPERSPARC_ICCR_ICE	1	/* Instruction cache enable */
#define	HYPERSPARC_ICCR_FTD	2	/* Unimpl. flush trap disable */
#define	HYPERSPARC_ASRNUM_ICCR	31	/* ICCR == ASR#31 */
