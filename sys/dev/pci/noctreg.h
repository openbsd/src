/*	$OpenBSD: noctreg.h,v 1.1 2002/06/02 18:15:03 jason Exp $	*/

/*
 * Copyright (c) 2002 Jason L. Wright (jason@thought.net)
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
 *	This product includes software developed by Jason L. Wright
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define	NOCT_BAR0		0x10		/* PCI base address */

#define	NOCT_BRDG_ENDIAN	0x0000		/* bridge endian mode */
#define	NOCT_BRDG_TIMER_PRESET	0x0004		/* bridge timer preset */
#define	NOCT_BRDG_STAT		0x0008		/* bridge status */
#define	NOCT_BRDG_CTL		0x000c		/* bridge control */
#define	NOCT_BRDG_TIMR		0x0010		/* bridge timer */
#define	NOCT_BRDG_TEST		0x0014		/* bridge test */
#define	NOCT_BRDG_RPMR		0x0018		/* bridge read ptr mirror */
#define	NOCT_EA_TEST_1		0x8000		/* e/a test 1 */
#define	NOCT_EA_TEST_0		0x8004		/* e/a test 0 */
#define	NOCT_EA_CMDQ_LEN	0x8008		/* e/a cmd queue len */
#define	NOCT_EA_CMDQ_PTR	0x800c		/* e/a cmd queue ptr */
#define	NOCT_EA_CMDQ_BAR1	0x8010		/* e/a cmd queue bar 1 */
#define	NOCT_EA_CMDQ_BAR0	0x8014		/* e/a cmd queue bar 0 */
#define	NOCT_EA_IER		0x8018		/* e/a intr enable */
#define	NOCT_EA_CSR		0x801c		/* e/a control/status */
#define	NOCT_EA_CTX_DAT_1	0x8020		/* e/a context data 1 */
#define	NOCT_EA_CTX_DAT_0	0x8024		/* e/a context data 0 */
#define	NOCT_EA_CTX_ADDR	0x8028		/* e/a context address */
#define	NOCT_EA_SDRAM_CFG	0x802c		/* e/a sdram config */
#define	NOCT_RNG_TOD		0xc000		/* rng time of day */
#define	NOCT_RNG_TOD_SCALE	0xc008		/* rng pre-scale */
#define	NOCT_RNG_TOD_READ	0xc010		/* rng time of day (read) */
#define	NOCT_RNG_X917_KEY1	0xc018		/* rng x9.17 key 1 */
#define	NOCT_RNG_X917_KEY2	0xc020		/* rng x9.17 key 2 */
#define	NOCT_RNG_HOSTSEED	0xc028		/* rng host seed */
#define	NOCT_RNG_SAMPBIAS	0xc030		/* rng intrl seed gen/bias */
#define	NOCT_RNG_EXTCLK_SCALE	0xc038		/* rng extrn clock scale */
#define	NOCT_RNG_WRITE		0xc040		/* rng write pointer */
#define	NOCT_RNG_SEEDSAMP	0xc048		/* rng seed sample */
#define	NOCT_RNG_LFSRDIAG	0xc050		/* rng lfsr diagnostics */
#define	NOCT_RNG_LFSRHIST_1	0xc058		/* rng lfsr history 1 */
#define	NOCT_RNG_LFSRHIST_2	0xc060		/* rng lfsr history 2 */
#define	NOCT_RNG_LFSRHIST_3	0xc068		/* rng lfsr history 3 */
#define	NOCT_RNG_LFSRHIST_4	0xc070		/* rng lfsr history 4 */
#define	NOCT_RNG_CTL		0xc078		/* rng control */
#define	NOCT_RNG_TEST_1		0xd000		/* rng test 1 */
#define	NOCT_RNG_TEST_0		0xd004		/* rng test 0 */
#define	NOCT_RNG_Q_LEN		0xd008		/* rng queue length */
#define	NOCT_RNG_Q_PTR		0xd00c		/* rng queue pointer */
#define	NOCT_RNG_BAR1		0xd010		/* rng bar1 */
#define	NOCT_RNG_BAR0		0xd014		/* rng bar0 */
#define	NOCT_RNG_CSR		0xd018		/* rng control/status */

#define	CTXADDR_READPEND	0x80000000	/* read pending/start */
#define	CTXADDR_WRITEPEND	0x40000000	/* write pending/start */
#define	CTXADDR_MASK		0x00ffffff	/* address mask */

/* NOCT_BRDG_STAT */
#define	BRDGSTS_PKP_INT		0x80000000	/* pkp interrupt */
#define	BRDGSTS_CCH_INT		0x40000000	/* cch interrupt */
#define	BRDGSTS_RNG_INT		0x20000000	/* rng interrupt */
#define	BRDGSTS_BRG_INT		0x10000000	/* bridge interrupt */
#define	BRDGSTS_TMR_INT		0x08000000	/* timer interrupt */
#define	BRDGSTS_CCH_ENA		0x01000000	/* mirror from e/a */
#define	BRDGSTS_CCH_BSY		0x00800000	/* mirror from e/a */
#define	BRDGSTS_CCH_ERR		0x00400000	/* mirror from e/a */
#define	BRDGSTS_CCH_RD_PEND	0x00200000	/* mirror from e/a */
#define	BRDGSTS_CCH_WR_PEND	0x00100000	/* mirror from e/a */
#define	BRDGSTS_PKH_ENA		0x00080000	/* mirror from pkh */
#define	BRDGSTS_PKH_BSY		0x00040000	/* mirror from pkh */
#define	BRDGSTS_PKH_ERR		0x00020000	/* mirror from pkh */
#define	BRDGSTS_PKH_SKS		0x00010000	/* mirror from pkh */
#define	BRDGSTS_HRESP_ERR	0x00002000	/* AHB slave HRESP error */
#define	BRDGSTS_HBURST_ERR	0x00001000	/* ccm, illegal burst */
#define	BRDGSTS_HSIZE_ERR	0x00000800	/* ccm, illegal size */
#define	BRDGSTS_PCIACC_ERR	0x00000400	/* pci access error */
#define	BRDGSTS_RSVMEM_ERR	0x00000200	/* reserved access */
#define	BRDGSTS_TRCVFIFO_PERR	0x00000100	/* CS6464AF parity error */
#define	BRDGSTS_PCIPERR		0x00000080	/* host parity error */

/* NOCT_BRDG_CTL */
#define	BRDGCTL_PKIRQ_ENA	0x80000000	/* pkh interrupt enable */
#define	BRDGCTL_EAIRQ_ENA	0x40000000	/* ea interrupt enable */
#define	BRDGCTL_RNIRQ_ENA	0x20000000	/* rng interrupt enable */
#define	BRDGCTL_BIRQ_ENA	0x10000000	/* bridge interrupt enable */
#define	BRDGCTL_TIRQ_ENA	0x08000000	/* timer interrupt enable */
#define	BRDGCTL_TIMER_ENA	0x00000001	/* enable timer */

/* NOCT_RNG_CTL */
#define	RNGCTL_RNG_ENA		0x80000000	/* rng enable */
#define	RNGCTL_TOD_ENA		0x40000000	/* enable tod counter */
#define	RNGCTL_EXTCLK_ENA	0x20000000	/* external clock enable */
#define	RNGCTL_DIAG		0x10000000	/* diagnostic mode */
#define	RNGCTL_BUFSRC_M		0x0c000000	/* buffer source: */
#define	RNGCTL_BUFSRC_X917	0x00000000	/*  X9.17 expander */
#define	RNGCTL_BUFSRC_SEED	0x04000000	/*  seed generator */
#define	RNGCTL_BUFSRC_HOST	0x08000000	/*  host data */
#define	RNGCTL_SEEDSRC_M	0x03000000	/* seed source: */
#define	RNGCTL_SEEDSRC_INT	0x00000000	/*  internal seed generator */
#define	RNGCTL_SEEDSRC_EXT	0x01000000	/*  external seed generator */
#define	RNGCTL_SEEDSRC_HOST	0x02000000	/*  host seed */
#define	RNGCTL_SEED_ERR		0x00008000	/* seed error */
#define	RNGCTL_X917_ERR		0x00004000	/* X9.17 error */
#define	RNGCTL_KEY1PAR_ERR	0x00002000	/* key 1 parity error */
#define	RNGCTL_KEY2PAR_ERR	0x00001000	/* key 2 parity error */
#define	RNGCTL_HOSTSEEDVALID	0x00000400	/* host seed not consumed */
#define	RNGCTL_BUF_RDY		0x00000200	/* buffer ready for write */
#define	RNGCTL_ITERCNT		0x000000ff	/* iteration count */

/* NOCT_RNG_CSR */
#define	RNGCSR_XFER_ENABLE	0x80000000	/* enable xfer queue */
#define	RNGCSR_XFER_BUSY	0x40000000	/* xfer in progress */
#define	RNGCSR_ERR_KEY		0x00800000	/* key error */
#define	RNGCSR_ERR_BUS		0x00400000	/* pci bus error */
#define	RNGCSR_ERR_DUP		0x00200000	/* duplicate block generated */
#define	RNGCSR_ERR_ACCESS	0x00100000	/* access error */
#define	RNGCSR_INT_KEY		0x00080000	/* intr ena: key error */
#define	RNGCSR_INT_BUS		0x00040000	/* intr ena: pci error */
#define	RNGCSR_INT_DUP		0x00020000	/* intr ena: dup error */
#define	RNGCSR_INT_ACCESS	0x00010000	/* intr ena: access error */

#define	RNGQPTR_READ_M		0x00007fff	/* read mask */
#define	RNGQPTR_READ_S		0		/* read shift */
#define	RNGQPTR_WRITE_M		0x7fff0000	/* write mask */
#define	RNGQPTR_WRITE_S		16		/* write shift */
