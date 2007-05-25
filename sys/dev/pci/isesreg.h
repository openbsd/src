/*	$OpenBSD: isesreg.h,v 1.10 2007/05/25 21:27:16 krw Exp $ $	*/

/*
 * Copyright (c) 2000 Håkan Olsson (ho@crt.se)
 * Copyright (c) 2000 Theo de Raadt
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * Register definitions for Pijnenburg PCC-ISES crypto chip.  
 * Definitions from revision 1.6 of the product datasheet.
 */

/* 
 * PCC-ISES Evaluation board DMA offsets 
 */
#define ISES_DMA_READ_COUNT	0x0100		/* bit 31-16 */
#define ISES_DMA_RCOUNT(x)	((x) << 16)
#define ISES_DMA_WRITE_COUNT	0x0100		/* bit 15-0  */
#define ISES_DMA_WCOUNT(x)	((x) & 0x00FF)

#define ISES_DMA_WRITE_START	0x0104
#define ISES_DMA_READ_START	0x0108
#define ISES_DMA_CTRL		0x010C
#define ISES_DMA_STATUS		ISES_DMA_CTRL
#define ISES_DMA_RESET		0x0110

#define ISES_DMA_CTRL_ILT	0x40000000	/* Ignore Latency Timer */
#define ISES_DMA_CTRL_RMULT	0x0D000000	/* Enable PCI Read Multiple */
#define ISES_DMA_CTRL_RLINE	0x09000000	/* Enable PCI Read Line */
#define ISES_DMA_CTRL_READ	0x01000000	/* Enable PCI Read */
#define ISES_DMA_CTRL_WRITE	0x00000100	/* Enable PCI Write */

#define ISES_DMA_STATUS_R_RUN	0x01000000	/* PCI Read running */
#define ISES_DMA_STATUS_R_ERR	0x02000000	/* PCI Read error */
#define ISES_DMA_STATUS_W_RUN	0x00000100	/* PCI Write running */
#define ISES_DMA_STATUS_W_ERR	0x00000200	/* PCI Write error */

/* 
 * PCC-ISES A-interface 
 */

#define ISES_A_OFFSET		0x0200
#define ISES_A(x)		(ISES_A_OFFSET + (x))

#define ISES_A_STAT		ISES_A(0x00)	/* status register */
#define ISES_A_INTS		ISES_A(0x04)	/* interrupt status register */
#define ISES_A_INTE		ISES_A(0x08)	/* interrupt enable register */
#define ISES_A_SREQ		ISES_A(0x0C)	/* service request (read) */
#define ISES_A_CTRL		ISES_A_SREQ	/* control register (write) */
#define ISES_A_OQD		ISES_A(0x10)	/* Output Queue Data (read) */
#define ISES_A_IQD		ISES_A_OQD	/* Input Queue Data (write) */
#define ISES_A_OQS		ISES_A(0x14)	/* Output Queue Semaphore */
#define ISES_A_IQS		ISES_A(0x18)	/* Input Queue Semaphore */
#define ISES_A_OQF		ISES_A(0x1C)	/* Output Queue Filled (ro) */
#define ISES_A_IQF		ISES_A(0x20)	/* Input Queue Free (ro) */

/*
 * PCC-ISES B-interface 
 */

#define ISES_B_OFFSET		0x0300
#define ISES_B(x)		(ISES_B_OFFSET + (x))

#define ISES_B_BDATAIN		ISES_B(0x0)
#define ISES_B_BDATAOUT		ISES_B(0x4)
#define ISES_B_STAT		ISES_B(0x8)

/*
 * PCC-ISES I-interface (not used)
 */

#define ISES_I_OFFSET		0x0400

/* 
 * PCC-ISES board registers
 */

#define ISES_BO_OFFSET		0x0500
#define ISES_BO(x)		(ISES_BO_OFFSET + (x))

#define ISES_BO_STAT		ISES_BO(0x0)
#define ISES_BO_LOOPCOUNTER	ISES_BO(0x4)
#define ISES_BO_TESTREG		ISES_BO(0x8)

#define ISES_BO_STAT_LOOP	0x00000001	/* B-interface LoopMode */
#define ISES_BO_STAT_TAMPER	0x00000002	/* Set tamper */
#define ISES_BO_STAT_POWERDOWN	0x00000004	/* Set power down */
#define ISES_BO_STAT_ACONF	0x00000008	/* Set A-intf access to 16b */
#define ISES_BO_STAT_HWRESET	0x00000010	/* Reset PCC-ISES (hw) */
#define ISES_BO_STAT_AIRQ	0x00000020	/* A-interface interrupt (ro)*/

/* 
 * PCC-ISES A-interface STAT register bits 
 */

#define ISES_STAT_LNAU_MASKED	0x00000001	/* LNAU flags masked, this bit
						   must be zero for the other
						   LNAU flags to be read
						   correctly. */
#define ISES_STAT_LNAU_BUSY_1	0x00000002	/* LNAU unit 1 is busy */
#define ISES_STAT_LNAU_ERR_1	0x00000004	/* LNAU unit 1 error */
#define ISES_STAT_LNAU_BUSY_2	0x00000008	/* LNAU unit 2 is busy */
#define ISES_STAT_LNAU_ERR_2	0x00000010	/* LNAU unit 2 error */
#define ISES_STAT_BCHU_MASKED	0x00000020	/* BCHU flags masked */
#define ISES_STAT_BCHU_BUSY	0x00000040	/* BCHU is busy */
#define ISES_STAT_BCHU_ERR	0x00000080	/* BCHU error flag */
#define ISES_STAT_BCHU_SCIF	0x00000100	/* symm. crypto inoperative */
#define ISES_STAT_BCHU_HIF	0x00000200	/* hash unit inoperative */
#define ISES_STAT_BCHU_DDB	0x00000400	/* discard data blocks */
#define ISES_STAT_BCHU_IRF	0x00000800	/* input request flag */
#define ISES_STAT_BCHU_OAF	0x00001000	/* output available flag */
#define ISES_STAT_BCHU_DIE	0x00002000	/* data input enabled */
#define ISES_STAT_BCHU_UE	0x00004000	/* unit enable bit */
#define ISES_STAT_BCHU_IFE	0x00008000	/* input FIFO empty */
#define ISES_STAT_BCHU_IFHE	0x00010000	/* input FIFO half emtpy */
#define ISES_STAT_BCHU_IFF	0x00020000	/* input FIFO full */
#define ISES_STAT_BCHU_OFE	0x00040000	/* output FIFO emtpy */
#define ISES_STAT_BCHU_OFHF	0x00080000	/* output FIFO half full */
#define ISES_STAT_BCHU_OFF	0x00100000	/* output FIFO full */
#define ISES_STAT_HW_DA		0x00200000	/* downloaded appl flag */
#define ISES_STAT_HW_ACONF	0x00400000	/* A-intf configuration flag */
#define ISES_STAT_SW_WFOQ	0x00800000	/* SW: Waiting for out queue */
#define ISES_STAT_SW_OQSINC	0x08000000	/* SW 2.x: OQS increased */

#define ISES_STAT_IDP_MASK	0x0f000000	/* IDP state mask (HW_DA=0) */
#define ISES_STAT_IDP_STATE(x)  (((x) & ISES_STAT_IDP_MASK) >> 24)
#define ISES_IDP_WFPL		0x4		/* Waiting for pgm len state */

static const char *ises_idp_state[] = 
{
	"reset state",				/* 0x0 */
	"testing NSRAM",			/* 0x1 */
	"checking for firmware",		/* 0x2 */
	"clearing NSRAM",			/* 0x3 */
	"waiting for program length",		/* 0x4 */
	"waiting for program data",		/* 0x5 */
	"waiting for program CRC",		/* 0x6 */
	"functional test program",		/* 0x7 */
	0, 0, 0, 0, 0, 0, 0,			/* 0x8-0xe */
	"Error: NSRAM or firmware failed"	/* 0xf */
};

#define ISES_STAT_SW_MASK	0x03000000	/* SW mode (HW_DA=1) */
#define ISES_STAT_SW_MODE(x)	(((x) & ISES_STAT_SW_MASK) >> 24)

#define ISES_A_CTRL_RESET	0x0000		/* SW reset (go to ST mode) */
#define ISES_A_CTRL_CONTINUE	0x0001		/* Return to CMD from WFC */

#ifdef ISESDEBUG
static const char *ises_sw_mode[] =
{
	"ST (SelfTest)",			/* 0x0 */
	"CMD",					/* 0x1 (normal) */
	"WFC (Wait for continue)",		/* 0x2 */
	"CMD (Wait for reset)"			/* 0x3 */
};
#endif

/* BERR (BCHU Error Register) */
#define ISES_BERR_DPAR		0x00000001	/* DES parity error */
#define ISES_BERR_IDESBCP	0x00000002	/* illegal DES mode value */
#define ISES_BERR_ISFRBCP	0x00000004	/* illegal SAFER rounds spec */
#define ISES_BERR_INCMBCP	0x00000008	/* illegal non-crypto mode */
#define ISES_BERR_IBCF		0x00000010	/* illegal value in BCFR */
#define ISES_BERR_reserved	0x00000020	/* reserved */
#define ISES_BERR_SRB		0x00000040	/* write SCU while busy */
#define ISES_BERR_HRB		0x00000080	/* write HU while busy */
#define ISES_BERR_IHFR		0x00000100	/* illegal value in HFR */
#define ISES_BERR_PADERR	0x00000200	/* padding error */
#define ISES_BERR_BIDM		0x00000400	/* B-interface input data
						   misalignment */
/* BCHCR (BCHU Control Register) */
#define ISES_BCHCR_BCHU_DIE	0x00000001	/* data input enabled */
#define ISES_BCHCR_BCHU_UE	0x00000002	/* unit enable */
#define ISES_BCHCR_BCHU_RST	0x00000004	/* BCHU reset */

/* 
 * OMR (Operation Method Register) 
 */
/* -- SELR (Selector Register) */
#define ISES_SELR_BCHU_EH	0x80000000	/* stop/continue on error */
#define ISES_SELR_BCHU_HISOF	0x01000000	/* HU input is SCU output */
#define ISES_SELR_BCHU_DIS	0x02000000	/* data interface select */

/* -- HOMR (HU Operation Mode Register) */
#define ISES_HOMR_HMTR		0x00800000	/* hash message type reg bit */
#define ISES_HOMR_ER		0x00300000	/* BE/LE, 2bit mask */

#define ISES_HOMR_HFR		0x00070000	/* Hash function mask, 3bits */
#define ISES_HOMR_HFR_NOP	0x00000000	/* NOP */
#define ISES_HOMR_HFR_MD5	0x00010000	/* MD5 */
#define ISES_HOMR_HFR_RMD160	0x00020000	/* RIPEMD-160 */
#define ISES_HOMR_HFR_RMD128	0x00030000	/* RIPEMD-128 */
#define ISES_HOMR_HFR_SHA1	0x00040000	/* SHA-1 */

/* -- SOMR (Symmetric crypto Operation Method Register) */
#define ISES_SOMR_BCFR		0x0000f000      /* block cipher function reg */
#define ISES_SOMR_BCPR		0x00000ff0	/* block cipher parameters */
#define ISES_SOMR_BOMR		(ISES_SOMR_BCFR | ISES_SOMR_BCPR)
#define ISES_SOMR_BOMR_NOP	0x00000000	/* NOP */
#define ISES_SOMR_BOMR_TRANSPARENT 0x00000010	/* Transparent */
#define ISES_SOMR_BOMR_DES	0x00001000	/* DES */
#define ISES_SOMR_BOMR_3DES2	0x00001010	/* 3DES-2 */
#define ISES_SOMR_BOMR_3DES	0x00001020	/* 3DES-3 */
#define ISES_SOMR_BOMR_SAFER	0x00002000	/* SAFER (actually more) */
#define ISES_SOMR_EDR		0x00000008	/* Encrypt/Decrypt register */
#define ISES_SOMR_FMR		0x00000003	/* feedback mode mask */
#define ISES_SOMR_FMR_ECB	0x00000000	/* EBC */
#define ISES_SOMR_FMR_CBC	0x00000001	/* CBC */
#define ISES_SOMR_FMR_CFB64	0x00000002	/* CFB64 */
#define ISES_SOMR_FMR_OFB64	0x00000003	/* OFB64 */

/* 
 * HRNG (Hardware Random Number Generator)
 */
#define ISES_OFFSET_HRNG_CTRL	0x00		/* Control register */
#define ISES_OFFSET_HRNG_LFSR	0x04		/* Linear feedback shift reg */
#define ISES_HRNG_CTRL_HE	0x00000001	/* HRNG enable */

/*
 * A-interface commands 
 */
#define ISES_MKCMD(cmd,len)	(cmd | cmd << 16 | len << 8 | len << 24)
#define ISES_CMD_NONE		-1

/*	Command name		Code	   Len	RLen    Desc                 */
#define ISES_CMD_CHIP_ID	0x00	/* 0	3	Read chipID */
/* LNAU commands - LNAU 1 */			
#define ISES_CMD_LRESET_1	0x01	/* 0	0	LNAU reset */
#define ISES_CMD_LRSFLG_1	0x02	/* 0	0	LNAU flags reset */
#define ISES_CMD_LUPLOAD_1	0x03	/* 0	64	Upload result */
#define ISES_CMD_LW_A_1		0x04	/* ?64	0	Load A register */
#define ISES_CMD_LW_B_1		0x05	/* ?64	0	Load B register */
#define ISES_CMD_LW_N_1		0x06	/* ?64	0	Load N register */
#define ISES_CMD_LW_Bq_1	0x07	/* ?32	0	Load Bq register */
#define ISES_CMD_LW_Nq_1	0x08	/* ?32	0	Load Nq register */
#define ISES_CMD_LW_Bp_1	0x09	/* ?34	0	Load Bp register */
#define ISES_CMD_LW_Np_1	0x0a	/* ?34	0	Load Np register */
#define ISES_CMD_LW_U_1		0x0b	/* ?34	0	Load U register */
#define ISES_CMD_LMOD_1		0x0c	/* 0	0	Start A % N */
#define ISES_CMD_LMULMOD_1	0x0d	/* 0	0	Start (A*B) % N */
#define ISES_CMD_LEXPMOD_1	0x0e	/* 0	0	Start (A^B) % N */
#define ISES_CMD_LEXPCRTMOD_1	0x0f	/* 0	0	Start (A^B)%N w/ CRT */
/* LNAU commands - LNAU 2 */
#define ISES_CMD_LRESET_2	0x10	/* 0	0	Reset */
#define ISES_CMD_LRSFLG_2	0x11	/* 0	0	Flags reset */
#define ISES_CMD_LUPLOAD_2	0x12	/* 0	64	Upload result */
#define ISES_CMD_LW_A_2		0x13	/* ?64	0	Load A register */
#define ISES_CMD_LW_B_2		0x14	/* ?64	0	Load B register */
#define ISES_CMD_LW_N_2		0x15	/* ?64	0	Load N register */
#define ISES_CMD_LW_Bq_2	0x16	/* ?32	0	Load Bq register */
#define ISES_CMD_LW_Nq_2	0x17	/* ?32	0	Load Nq register */
#define ISES_CMD_LW_Bp_2	0x18	/* ?34	0	Load Bp register */
#define ISES_CMD_LW_Np_2	0x19	/* ?34	0	Load Np register */
#define ISES_CMD_LW_U_2		0x1a	/* ?34	0	Load U register */
#define ISES_CMD_LMOD_2		0x1b	/* 0	0	Start A % N */
#define ISES_CMD_LMULMOD_2	0x1c	/* 0	0	Start (A*B) % N */
#define ISES_CMD_LEXPMOD_2	0x1d	/* 0	0	Start (A^B) % N */
#define ISES_CMD_LEXPCRTMOD_2	0x1e	/* 0	0	Start (A^B)%N w/ CRT */
/* BCHU commands */
#define ISES_CMD_RST_BERR	0x1f	/* 0	0	Reset BERR */
#define ISES_CMD_BR_BERR	0x20	/* 0	0	Read BERR */
#define ISES_CMD_BW_DATA	0x21	/* 2	0	Write DATA */
#define ISES_CMD_BR_DATA	0x22	/* 0	2	Read DATA */
#define ISES_CMD_BW_BCHCR	0x23	/* 1	0	Write BCHCR */
#define ISES_CMD_BR_BCHCR	0x24	/* 0	0	Read BCHCR */
#define ISES_CMD_BW_OMR		0x25	/* 1	0	Write OMR */
#define ISES_CMD_BR_OMR		0x26	/* 0	1	Read OMR */
#define ISES_CMD_BW_KR0		0x27	/* 2	0	Write key 0 */
#define ISES_CMD_BR_KR0		0x28	/* 0	2	Read key 0 */
#define ISES_CMD_BW_KR1		0x29	/* 2	0	Write key 1 */
#define ISES_CMD_BR_KR1		0x2a	/* 0	2	Read key 1 */
#define ISES_CMD_BW_KR2		0x2b	/* 2	0	Write key 2 */
#define ISES_CMD_BR_KR2		0x2c	/* 0	2	Read key 2 */
#define ISES_CMD_BW_SCCR	0x2d	/* 2	0	Write SCCR */
#define ISES_CMD_BR_SCCR	0x2e	/* 0	2	Read SCCR */
#define ISES_CMD_BW_DBCR	0x2f	/* 2	0	Write DBCR */
#define ISES_CMD_BR_DBCR	0x30	/* 0	2	Read DBCR */
#define ISES_CMD_BW_HMLR	0x31	/* 2	0	Write HMLR */
#define ISES_CMD_BR_HMLR	0x32	/* 0	2	Read HMLR */
#define ISES_CMD_BW_CVR		0x33	/* 5	0	Write CVR */
#define ISES_CMD_BR_CVR		0x34	/* 0	5	Read CVR */
#define ISES_CMD_BPROC		0x35	/* ?255	?255	Process data blocks */
#define ISES_CMD_BTERM		0x36	/* 0	0	Terminate session */
#define ISES_CMD_BSWITCH	0x37	/* 18	18	Switch BCHU session */
/* HRNG commands */
#define ISES_CMD_HSTART		0x38	/* 0	0	Start RBG unit */
#define ISES_CMD_HSTOP		0x39	/* 0	0	Stop RGB unit */
#define ISES_CMD_HSEED		0x3a	/* 1	0	Seed LFSR */
#define ISES_CMD_HBITS		0x3b	/* 1	?255	Return n*32 rnd bits */

/* Command return codes (RC) */
#define ISES_RC_MASK		0x0000ffff	
#define ISES_RC_SUCCESS		0x0000		/* success */
#define ISES_RC_CMDERR		0x0001		/* cmd interpretation error */
#define ISES_RC_QERR		0x0002		/* queue handling error */
#define ISES_RC_LNAU_ERR	0x0003		/* LNAU cmd proc error */
#define ISES_RC_BCHU_ERR	0x0004		/* BCHU cmd proc error */
#define ISES_RC_BCHU_BIFCSEL	0x0005		/* OMR says B-if, must be A */
#define ISES_RC_BCHU_ODD	0x0006		/* odd #words in param list */
#define ISES_RC_HRNG_ILLEN	0x0007		/* too large bitstream */

/* Interrupt bits, IRQE, IRQES, IRQEC, IRQSS, IRQ registers */
#define ISES_IRQ_TIMER_1	0x00000001	/* Timer 1 reached zero */
#define ISES_IRQ_TIMER_2	0x00000002	/* Timer 2 reached zero */
#define ISES_IRQ_I_IIN0		0x00000004	/* I-int 'Iin0' */
#define ISES_IRQ_I_IIN1		0x00000008	/* I-int 'Iin1' */
#define ISES_IRQ_I_IIN2		0x00000010	/* I-int 'Iin2' */
#define ISES_IRQ_I_IIN3		0x00000020	/* I-int 'Iin3' */
#define ISES_IRQ_LNAU_1_ERROR	0x00000040	/* LNAU 1 op error/abort */
#define ISES_IRQ_LNAU_1_DONE	0x00000080	/* LNAU 1 op done */
#define ISES_IRQ_LNAU_2_ERROR	0x00000100	/* LNAU 2 op error/abort */
#define ISES_IRQ_LNAU_2_DONE	0x00000200	/* LNAU 1 op done */
#define ISES_IRQ_BCHU_DONE	0x00000400	/* BCHU operation done */
#define ISES_IRQ_BCHU_ERROR	0x00000800	/* BCHU operation error/abrt */
#define ISES_IRQ_BCHU_IRF	0x00001000	/* BCHU input request flag >1*/
#define ISES_IRQ_BCHU_OAF	0x00002000	/* BCHU output avail flag >1 */
#define ISES_IRQ_BCHU_IEF	0x00004000	/* BCHU input empty flag >1  */
#define ISES_IRQ_A_WCTRL	0x00008000	/* A-int CTRL reg was written*/
#define ISES_IRQ_A_RSREQ	0x00010000	/* A-int SREQ reg was read   */
#define ISES_IRQ_A_DIQ		0x00020000	/* in queue emtpy, IQD write */
#define ISES_IRQ_A_CIQ		0x00040000	/* in queue has complete cmd */
#define ISES_IRQ_A_OQF		0x00080000	/* output queue full */

#define ISES_SESSION(sid)	( (sid) & 0x0fffffff)
#define	ISES_CARD(sid)		(((sid) & 0xf0000000) >> 28)
#define	ISES_SID(crd,ses)	(((crd) << 28) | ((ses) & 0x0fffffff))

/* Size and layout of ises_session is firmware dependent. */
/* This structure should be usable for the SWITCH_SESSION command. */
struct ises_session {
	u_int32_t	kr[6];		/* Key register KR2,KR1,KR0 */
	u_int32_t	omr;		/* Operation method register */

	/* The following values (on-chip) are cleared after an OMR write */
	u_int32_t	sccr[2];	/* Symm. crypto chaining reg. (IV) */
	u_int32_t	cvr[5];		/* Chaining variables reg. */
	u_int32_t	dbcr[2];	/* Data block count register */
	u_int32_t	hmlr[2];	/* Hash message length reg. */
} __attribute__((packed));

#define ISES_B_DATASIZE			4096
