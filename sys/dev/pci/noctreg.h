/*	$OpenBSD: noctreg.h,v 1.8 2003/06/02 19:08:58 jason Exp $	*/

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
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#define	NOCT_BAR0		0x10		/* PCI base address */

#define	NOCT_BRDG_ENDIAN	0x0000		/* bridge endian mode */
#define	NOCT_BRDG_TIMER_PRESET	0x0004		/* bridge timer preset */
#define	NOCT_BRDG_STAT		0x0008		/* bridge status */
#define	NOCT_BRDG_CTL		0x000c		/* bridge control */
#define	NOCT_BRDG_TIMR		0x0010		/* bridge timer */
#define	NOCT_BRDG_TEST		0x0014		/* bridge test */
#define	NOCT_BRDG_RPMR		0x0018		/* bridge read ptr mirror */
#define	NOCT_PKH_TEST0		0x4000		/* pkh test 0 */
#define	NOCT_PKH_TEST1		0x4004		/* pkh test 1 */
#define	NOCT_PKH_Q_LEN		0x4008		/* pkh queue length */
#define	NOCT_PKH_Q_PTR		0x400c		/* pkh queue ptrs */
#define	NOCT_PKH_Q_BASE_HI	0x4010		/* pkh queue base, msw */
#define	NOCT_PKH_Q_BASE_LO	0x4014		/* pkh queue base, lsw */
#define	NOCT_PKH_IER		0x4018		/* pkh intr enable */
#define	NOCT_PKH_CSR		0x401c		/* pkh control/status */
#define	NOCT_PKH_SKS_DATA	0x4020		/* pkh secure key data */
#define	NOCT_PKH_SKS_CTRL	0x4024		/* pkh secure key ctrl */
#define	NOCT_PKH_CMDB_0		0x40e0		/* pkh command block 0 */
#define	NOCT_PKH_CMDB_1		0x40e4		/* pkh command block 1 */
#define	NOCT_PKH_CMDB_2		0x40e8		/* pkh command block 2 */
#define	NOCT_PKH_CMDB_3		0x40ec		/* pkh command block 3 */
#define	NOCT_PKH_CMDB_4		0x40f0		/* pkh command block 4 */
#define	NOCT_PKH_CMDB_5		0x40f4		/* pkh command block 5 */
#define	NOCT_PKH_CMDB_6		0x40f8		/* pkh command block 6 */
#define	NOCT_PKH_CMDB_7		0x40fc		/* pkh command block 7 */
#define	NOCT_PKH_BNCACHE_START	0x5000		/* pkh bignumber cache start */
#define	NOCT_PKH_BNCACHE_END	0x5fff		/* pkh bigbumber cache end */
#define	NOCT_EA_TEST_1		0x8000		/* e/a test 1 */
#define	NOCT_EA_TEST_0		0x8004		/* e/a test 0 */
#define	NOCT_EA_Q_LEN		0x8008		/* e/a cmd queue len */
#define	NOCT_EA_Q_PTR		0x800c		/* e/a cmd queue ptr */
#define	NOCT_EA_Q_BASE_HI	0x8010		/* e/a cmd queue bar 1 */
#define	NOCT_EA_Q_BASE_LO	0x8014		/* e/a cmd queue bar 0 */
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
#define	NOCT_RNG_Q_BASE_HI	0xd010		/* rng bar1 */
#define	NOCT_RNG_Q_BASE_LO	0xd014		/* rng bar0 */
#define	NOCT_RNG_CSR		0xd018		/* rng control/status */

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

/* NOCT_PKH_Q_LEN */
#define	PKHQLEN_MASK		0x0000000f	/* queue length, 2^n */

/* NOCT_PKH_Q_PTR */
#define PKHQPTR_READ_M		0x7fff0000	/* read mask */
#define	PKHQPTR_READ_S		16		/* read shift */
#define	PKHQPTR_WRITE_M		0x00007fff	/* write mask */
#define	PKHQPTR_WRITE_S		0		/* write shift */

/* NOCT_PKH_IER */
#define	PKHIER_CMDSI		0x00020000	/* cmd successful, SI */
#define	PKHIER_SKSWR		0x00010000	/* sks write op done */
#define	PKHIER_SKSOFF		0x00008000	/* sks offset error */
#define	PKHIER_PKHLEN		0x00004000	/* invalid data length */
#define	PKHIER_PKHOPCODE	0x00002000	/* invalid opcode */
#define	PKHIER_BADQBASE		0x00001000	/* base queue base */
#define	PKHIER_LOADERR		0x00000800	/* bus error during load */
#define	PKHIER_STOREERR		0x00000400	/* bus error during store */
#define	PKHIER_CMDERR		0x00000200	/* bus error during cmd */
#define	PKHIER_ILL		0x00000100	/* illegal access */
#define	PKHIER_PKERESV		0x00000080	/* pke reserved */
#define	PKHIER_PKEWDT		0x00000040	/* pke mul inv watchdog */
#define	PKHIER_PKENOTPRIME	0x00000020	/* pke not relatively prime */
#define	PKHIER_PKE_B		0x00000010	/* pke bad 'b' error */
#define	PKHIER_PKE_A		0x00000008	/* pke bad 'a' error */
#define	PKHIER_PKE_M		0x00000004	/* pke bad 'm' error */
#define	PKHIER_PKE_R		0x00000002	/* pke bad 'r' error */
#define	PKHIER_PKEOPCODE	0x00000001	/* pke bad opcode */

/* NOCT_PKH_CSR */
#define	PKHCSR_PKH_ENA		0x80000000	/* enable pkh */
#define	PKHCSR_PKH_BUSY		0x40000000	/* pkh busy */
#define	PKHCSR_PKE_GO		0x20000000	/* pke go input */
#define	PKHCSR_PKE_BUSY		0x10000000	/* pke busy output */
#define	PKHCSR_LINENO		0x0f000000	/* pke error opcode */
#define	PKHCSR_CMDSI		0x00020000	/* cmd successful, SI */
#define	PKHCSR_SKSWR		0x00010000	/* sks write op done */
#define	PKHCSR_SKSOFF		0x00008000	/* sks offset error */
#define	PKHCSR_PKHLEN		0x00004000	/* invalid data length */
#define	PKHCSR_PKHOPCODE	0x00002000	/* invalid opcode */
#define	PKHCSR_BADQBASE		0x00001000	/* base queue base */
#define	PKHCSR_LOADERR		0x00000800	/* bus error during load */
#define	PKHCSR_STOREERR		0x00000400	/* bus error during store */
#define	PKHCSR_CMDERR		0x00000200	/* bus error during cmd */
#define	PKHCSR_ILL		0x00000100	/* illegal access */
#define	PKHCSR_PKERESV		0x00000080	/* pke reserved */
#define	PKHCSR_PKEWDT		0x00000040	/* pke mul inv watchdog */
#define	PKHCSR_PKENOTPRIME	0x00000020	/* pke not relatively prime */
#define	PKHCSR_PKE_B		0x00000010	/* pke bad 'b' error */
#define	PKHCSR_PKE_A		0x00000008	/* pke bad 'a' error */
#define	PKHCSR_PKE_M		0x00000004	/* pke bad 'm' error */
#define	PKHCSR_PKE_R		0x00000002	/* pke bad 'r' error */
#define	PKHCSR_PKEOPCODE	0x00000001	/* pke bad opcode */

/* NOCT_PKH_SKS_CTRL */
#define	PKHSKS_GO		0x80000000	/* pkhsks busy */
#define	PKHSKS_PROMERR		0x40000000	/* prom protocol error */
#define	PKHSKS_ACCERR		0x20000000	/* access error */
#define	PKHSKS_LOCMASK		0x00003000	/* location mask: */
#define	PKHSKS_LOC_FROMPROM	0x00002000
#define	PKHSKS_LOC_CACHEONLY	0x00001000
#define	PKHSKS_ADDR		0x00000fff	/* address mask */

/*
 * public key structures
 */
/* opcodes/flags */
#define	PKH_OP_CODE_MASK	0xf0000000	/* opcode mask */
#define	PKH_OP_CODE_MOD		0x00000000	/* a mod m */
#define	PKH_OP_CODE_RMOD	0x10000000	/* R mod m */
#define	PKH_OP_CODE_ADD		0x20000000	/* (a + b) mod m */
#define	PKH_OP_CODE_SUB		0x30000000	/* (a - b) mod m */
#define	PKH_OP_CODE_ADDINV	0x40000000	/* -a mod m */
#define	PKH_OP_CODE_MUL		0x50000000	/* (a * b) mod m */
#define	PKH_OP_CODE_MULINV	0x60000000	/* 1/a mod m */
#define	PKH_OP_CODE_EXP		0x70000000	/* g^e mod m */
#define	PKH_OP_CODE_LOAD	0x80000000	/* load bn cache */
#define	PKH_OP_CODE_STORE	0x90000000	/* store bn cache */
#define	PKH_OP_CODE_RSAPRIV	0xa0000000	/* rsa private key op */
#define	PKH_OP_CODE_DSASIGN	0xb0000000	/* dsa sign op */
#define	PKH_OP_CODE_NOP		0xf0000000	/* no-op */
#define	PKH_OP_SI		0x08000000	/* set interrupt */

/* pkh arithmetic commands */
struct noct_pkh_cmd_arith {
	volatile u_int32_t	op;		/* opcode/si, 0x0 - 0x7 */
	volatile u_int32_t	r;		/* r offset */
	volatile u_int32_t	m;		/* m length, m offset */
	volatile u_int32_t	a;		/* a length, a offset */
	volatile u_int32_t	b;		/* b length, b offset */
	volatile u_int32_t	c;		/* c offset */
	volatile u_int32_t	unused[2];	/* reserved */
};

/* pkh load/store bn cache commands */
struct noct_pkh_cmd_cache {
	volatile u_int32_t	op;		/* opcode/si, 0x8-0x9 */
	volatile u_int32_t	r;		/* r offset */
	volatile u_int32_t	addrhi;		/* host address, msw */
	volatile u_int32_t	addrlo;		/* host address, lsw */
	volatile u_int32_t	len;		/* data length (0-4096) */
	volatile u_int32_t	unused[3];	/* reserved */
};

/* pkh rsa private command */
struct noct_pkh_cmd_rsapriv {
	volatile u_int32_t	op;		/* opcode/si, 0xa */
	volatile u_int32_t	par;		/* n, keylen, sksoffset */
	volatile u_int32_t	unused[6];	/* reserved */
};

/* pkh dsa sign command */
struct noct_pkh_cmd_dsasign {
	volatile u_int32_t	op;		/* opcode/si, 0xb */
	volatile u_int32_t	par;		/* n, keylen, sksoffset */
	volatile u_int32_t	unused[6];	/* reserved */
};

/* pkh nop command */
struct noct_pkh_cmd_nop {
	volatile u_int32_t	op;		/* opcode/si, 0xf */
	volatile u_int32_t	unused[7];	/* reserved */
};

/* pkh generic command */
union noct_pkh_cmd {
	struct noct_pkh_cmd_arith	arith;
	struct noct_pkh_cmd_cache	cache;
	struct noct_pkh_cmd_rsapriv	rsapriv;
	struct noct_pkh_cmd_dsasign	dsasign;
	struct noct_pkh_cmd_nop		nop;
};

/* NOCT_EA_Q_LEN */
#define	EAQLEN_MASK		0x0000000f	/* queue length, 2^n */

/* NOCT_EA_Q_PTR */
#define EAQPTR_READ_M		0x7fff0000	/* read mask */
#define	EAQPTR_READ_S		16		/* read shift */
#define	EAQPTR_WRITE_M		0x00007fff	/* write mask */
#define	EAQPTR_WRITE_S		0		/* write shift */

/* NOCT_EA_IER */
#define	EAIER_QALIGN		0x00008000	/* queue alignment */
#define	EAIER_CMDCMPL		0x00004000	/* command complete */
#define	EAIER_OPERR		0x00002000	/* opcode error */
#define	EAIER_CMDREAD		0x00001000	/* command read error */
#define	EAIER_CMDWRITE		0x00000800	/* command write error */
#define	EAIER_DATAREAD		0x00000400	/* data read error */
#define	EAIER_DATAWRITE		0x00000200	/* data write error */
#define	EAIER_INTRNLLEN		0x00000100	/* internal data length err */
#define	EAIER_EXTRNLLEN		0x00000080	/* external data length err */
#define	EAIER_DESBLOCK		0x00000040	/* des block size error */
#define	EAIER_DESKEY		0x00000020	/* des key error */
#define	EAIER_ILL		0x00000001	/* illegal access */

/* NOCT_EA_CSR */
#define	EACSR_ENABLE		0x80000000	/* e/a enable */
#define	EACSR_BUSY		0x40000000	/* e/a busy */
#define	EACSR_QALIGN		0x00008000	/* queue alignment */
#define	EACSR_CMDCMPL		0x00004000	/* command complete */
#define	EACSR_OPERR		0x00002000	/* opcode error */
#define	EACSR_CMDREAD		0x00001000	/* command read error */
#define	EACSR_CMDWRITE		0x00000800	/* command write error */
#define	EACSR_DATAREAD		0x00000400	/* data read error */
#define	EACSR_DATAWRITE		0x00000200	/* data write error */
#define	EACSR_INTRNLLEN		0x00000100	/* internal data length err */
#define	EACSR_EXTRNLLEN		0x00000080	/* external data length err */
#define	EACSR_DESBLOCK		0x00000040	/* des block size error */
#define	EACSR_DESKEY		0x00000020	/* des key error */
#define	EACSR_ILL		0x00000001	/* illegal access */

/* NOCT_EA_CTX_ADDR */
#define	EACTXADDR_READPEND	0x80000000	/* read pending/start */
#define	EACTXADDR_WRITEPEND	0x40000000	/* write pending/start */
#define	EACTXADDR_MASK		0x00ffffff	/* address mask */

/* NOCT_EA_SDRAM_CFG */
#define	EASDRC_8KREFRESH	0x00000080	/* 8K refreshes/64ms */
#define	EASDRC_FREQ		0x0000003f	/* in MHz */

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

/* NOCT_RNG_Q_PTR */
#define	RNGQPTR_READ_M		0x00007fff	/* read mask */
#define	RNGQPTR_READ_S		0		/* read shift */
#define	RNGQPTR_WRITE_M		0x7fff0000	/* write mask */
#define	RNGQPTR_WRITE_S		16		/* write shift */

#define	EA_CMD_WORDS		32
struct noct_ea_cmd {
	volatile u_int32_t	buf[EA_CMD_WORDS];
};


#define	EA_0_CP			0x80000000	/* context block */
#define	EA_0_SI			0x40000000	/* set interrupt */
#define	EA_0_CTXIDX		0x0003ffff	/* context index */

#define	EA_1_OPCODE		0xff000000	/* opcode */
#define	EA_1_ITERCNT		0x000f0000	/* iteration count */
#define	EA_1_DATALEN		0x0000ffff	/* data length (bytes) */

#define	EA_OP_NOP		0x00000000	/* nop */
#define	EA_OP_WCTX		0x01000000	/* write context mem */
#define	EA_OP_RCTX		0x02000000	/* read context mem */
#define	EA_OP_3DESCBCE		0x03000000	/* 3DES, CBC, encrypt */
#define	EA_OP_3DESCBCD		0x04000000	/* 3DES, CBC, decrypt */
#define	EA_OP_ARC4		0x05000000	/* ARC4 transform */
#define	EA_OP_MD5		0x10000000	/* simple MD5 hash */
#define	EA_OP_MD5_APP		0x11000000	/* MD5 hash w/append */
#define	EA_OP_MD5_CMDIV		0x12000000	/* MD5 hash, IV from cmd */
#define	EA_OP_MD5_LSTIV		0x13000000	/* MD5 hash, IV from last */
#define	EA_OP_MD5_LSTCMD	0x14000000	/* MD5 hashlast;iv from cmd */
#define	EA_OP_MD5_LSTLST	0x15000000	/* MD5 hashlast;iv from last */
#define	EA_OP_MD5_KEYMAT	0x16000000	/* MD5 KEYMAT expansion */
#define	EA_OP_MD5_SKEYID	0x17000000	/* MD5 SKEYID expansion */
#define	EA_OP_MD5_IKEKEY	0x18000000	/* MD5 IKE KEYMAT */
#define	EA_OP_SHA1		0x20000000	/* simple SHA1 hash */
#define	EA_OP_SHA1_APP		0x21000000	/* SHA1 hash w/append */
#define	EA_OP_SHA1_CMDIV	0x22000000	/* SHA1 hash, IV from cmd */
#define	EA_OP_SHA1_LSTIV	0x23000000	/* SHA1 hash, IV from last */
#define	EA_OP_SHA1_LSTCMD	0x24000000	/* SHA1 hashlast;iv from cmd */
#define	EA_OP_SHA1_LSTLST	0x25000000	/* SHA1 hashlast;iv from lst */
#define	EA_OP_SHA1_KEYMAT	0x26000000	/* SHA1 KEYMAT expansion */
#define	EA_OP_SHA1_SKEYID	0x27000000	/* SHA1 SKEYID expansion */
#define	EA_OP_SHA1_IKEKEY	0x28000000	/* SHA1 IKE KEYMAT */
#define	EA_OP_MASTER_HASH	0x30000000	/* master secret hash */
#define	EA_OP_NULL_NULL		0x80000000	/* packet level null/null */
#define	EA_OP_SSL3_ARC4_MD5_E	0x81000000	/* ssl 3.0 arc4/md5 enc */
#define	EA_OP_SSL3_DES_MD5_E	0x82000000	/* ssl 3.0 des/md5 enc */
#define	EA_OP_SSL3_ARC4_SHA1_E	0x83000000	/* ssl 3.0 arc4/sha1 enc */
#define	EA_OP_SSL3_DES_SHA1_E	0x84000000	/* ssl 3.0 des/sha1 enc */
#define	EA_OP_SSL3_ARC4_MD5_D	0x91000000	/* ssl 3.0 arc4/md5 dec */
#define	EA_OP_SSL3_DES_MD5_D	0x92000000	/* ssl 3.0 des/md5 dec */
#define	EA_OP_SSL3_ARC4_SHA1_D	0x93000000	/* ssl 3.0 arc4/sha1 dec */
#define	EA_OP_SSL3_DES_SHA1_D	0x94000000	/* ssl 3.0 des/sha1 dec */
#define	EA_OP_TLS1_ARC4_MD5_E	0xa1000000	/* tls 1.0 arc4/md5 enc */
#define	EA_OP_TLS1_DES_MD5_E	0xa2000000	/* tls 1.0 des/md5 enc */
#define	EA_OP_TLS1_ARC4_SHA1_E	0xa3000000	/* tls 1.0 arc4/sha1 enc */
#define	EA_OP_TLS1_DES_SHA1_E	0xa4000000	/* tls 1.0 des/sha1 enc */
#define	EA_OP_TLS1_ARC4_MD5_D	0xb1000000	/* tls 1.0 arc4/md5 dec */
#define	EA_OP_TLS1_DES_MD5_D	0xb2000000	/* tls 1.0 des/md5 dec */
#define	EA_OP_TLS1_ARC4_SHA1_D	0xb3000000	/* tls 1.0 arc4/sha1 dec */
#define	EA_OP_TLS1_DES_SHA1_D	0xb4000000	/* tls 1.0 des/sha1 dec */
#define	EA_OP_IPS_DES_MD5_E	0xc1000000	/* ipsec des/md5 enc */
#define	EA_OP_IPS_DES_SHA1_E	0xc2000000	/* ipsec des/sha1 enc */
#define	EA_OP_IPS_DES_MD5_D	0xd1000000	/* ipsec des/md5 dec */
#define	EA_OP_IPS_DES_SHA1_D	0xd2000000	/* ipsec des/sha1 dec */
