/*	$OpenBSD: vsreg.h,v 1.8 2004/05/22 19:34:12 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
 * Copyright (c) 1999 Steve Murphree, Jr.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from source contributed by Mark Bellon.
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
 */

#ifndef	_MVME328_REG_H_
#define	_MVME328_REG_H_

/*
 * JAGUAR specific device limits
 */

#define	JAGUAR_MIN_Q_SIZ		2
#define	JAGUAR_MAX_Q_SIZ		2
#define	JAGUAR_MAX_CTLR_CMDS		80	/* Interphase says so */

/*
 * COUGAR specific device limits
 */

#define	COUGAR_MIN_Q_SIZ		2
#define	COUGAR_CMDS_PER_256K		42	/* Interphase says so */

/*
 * Structures
 */

#define	NUM_CQE			10
#define	NUM_IOPB		NUM_CQE

/*
 * Master Control Status Block (MCSB)
 */

#define	MCSB_MSR		0x000000	/* master status register */
#define	M_MSR_QFC	0x0004		/* queue flush complete */
#define	M_MSR_BOK	0x0002		/* board OK */
#define	M_MSR_CNA	0x0001		/* controller not available */
#define	MCSB_MCR		0x000002	/* master control register */
#define	M_MCR_SFEN	0x2000		/* sysfail enable */
#define	M_MCR_RES	0x1000		/* reset controller */
#define	M_MCR_FLQ	0x0800		/* flush queue */
#define	M_MCR_FLQR	0x0004		/* flush queue and report */
#define	M_MCR_SQM	0x0001		/* start queue mode */
#define	MCSB_IQAR		0x000004	/* interrupt on queue avail */
#define	M_IQAR_IQEA	0x8000		/* interrupt on queue entry avail */
#define	M_IQAR_IQEH	0x4000		/* interrupt on queue half empty */
#define	M_IQAR_ILVL	0x0700		/* interrupt lvl on queue available */
#define	M_IQAR_IVCT	0x00ff		/* interrupt vector on queue avail */
#define	MCSB_QHDP		0x000006	/* queue head pointer */
#define	MCSB_THAW		0x000008	/* thaw work queue */
#define	M_THAW_TWQN	0xff00		/* thaw work queue number */
#define	M_THAW_TWQE	0x0001		/* thaw work queue enable */
#define	MCSB_SIZE		0x000010

/*
 * Host Semaphore Block (HSB)
 */

#define	HSB_INITQ		0x000000	/* init MCE flag */
#define	HSB_WORKQ		0x000002	/* work queue number */
#define	HSB_MAGIC		0x000004	/* magic word */
#define	HSB_SIZE		0x000008

/*
 * Controller Initialization Block (CIB)
 */

#define	CIB_NCQE		0x000000	/* number of CQE */
#define	CIB_BURST		0x000002	/* DMA burst count */
#define	CIB_NVECT		0x000004	/* normal completion vector */
#define	CIB_EVECT		0x000006	/* error completion vector */
#define	M_VECT_ILVL	0x0700		/* Interrupt Level mask */
#define	M_VECT_IVCT	0x00ff		/* Interrupt Vector mask */
#define	CIB_PID			0x000008	/* primary scsi bus id */
#define	CIB_SID			0x00000a	/* secondary scsi bus id */
#define	M_PSID_DFT	0x0008 		/* default ID enable */
#define	M_PSID_ID	0x0007		/* primary/secondary SCSI ID */
#define	CIB_CRBO		0x00000c	/* CRB offset */
#define	CIB_SELECT		0x00000e	/* selection timeout in ms */
#define	CIB_WQTIMO		0x000012	/* work queue timeout in 256ms */
#define	CIB_VMETIMO		0x000016	/* VME timeout in 32ms */
#define	CIB_OBMT		0x00001e	/* offboard CRB mtype/xfer type/ad mod */
#define	CIB_OBADDR		0x000020	/* host mem address for offboard CRB */
#define	CIB_ERR_FLGS		0x000024	/* error recovery flags */
#define	M_ERRFLGS_FOSR	0x0001		/* Freeze on SCSI bus reset */
#define	M_ERRFLGS_RIN	0x0002		/* SCSI bus reset interrupt */
#define	M_ERRFLGS_RSE	0x0004		/* Report COUGAR SCSI errors */
#define	CIB_SBRIV		0x00002a	/* scsi bus reset interrupt vector */
#define	CIB_SOF0		0x00002c	/* synchronous offset (bus 0) */
#define	CIB_SRATE0		0x00002d	/* sync negotiation rate (bus 0) */
#define	CIB_SOF1		0x00002e	/* synchronous offset (bus 0) */
#define	CIB_SRATE1		0x00002f	/* sync negotiation rate (bus 0) */
#define	CIB_SIZE		0x000030

/*
 * Command Queue Entry (CQE)
 */

#define	CQE_QECR		0x000000	/* queue entry control reg */
#define	M_QECR_IOPB	0x0f00		/* IOPB type (must be zero) */
#define	M_QECR_HPC	0x0004		/* High Priority command */
#define	M_QECR_AA	0x0002		/* abort acknowledge */
#define	M_QECR_GO	0x0001		/* Go/Busy */
#define	CQE_IOPB_ADDR		0x000002	/* IOPB address */
#define	CQE_CTAG		0x000004	/* command tag */
#define	CQE_IOPB_LENGTH		0x000008	/* IOPB length */
#define	CQE_WORK_QUEUE		0x000009	/* work queue number */
#define	CQE_SIZE		0x00000c

/*
 * Command Response Block (CRB)
 */

#define	CRB_CRSW		0x000000	/* status word */
#define	M_CRSW_SE	0x0800		/* SCSI error (COUGAR) */
#define	M_CRSW_RST	0x0400		/* SCSI Bus reset (COUGAR) */
#define	M_CRSW_SC	0x0080		/* status change */
#define	M_CRSW_CQA	0x0040		/* Command queue entry available */
#define	M_CRSW_QMS	0x0020		/* queue mode started */
#define	M_CRSW_AQ	0x0010		/* abort queue */
#define	M_CRSW_EX	0x0008		/* exception */
#define	M_CRSW_ER	0x0004		/* error */
#define	M_CRSW_CC	0x0002		/* command complete */
#define	M_CRSW_CRBV	0x0001		/* cmd response block valid/clear */
#define	CRB_CTAG		0x000004	/* command tag */
#define	CRB_IOPB_LENGTH		0x000008	/* IOPB length */
#define	CRB_WORK_QUEUE		0x000009	/* work queue number */
#define	CRB_SIZE		0x00000c

/*
 * Controller Error Vector Status Block (CEVSB)
 * This is a variation of the CRB, _but larger_.
 */

#define	CONTROLLER_ERROR	0x0085
#define	NR_SCSI_ERROR		0x0885

#define	CEVSB_CRSW		0x000000	/* status word */
#define	CEVSB_TYPE		0x000002	/* IOPB type */
#define	CEVSB_CTAG		0x000004	/* command tag */
#define	CEVSB_IOPB_LENGTH	0x000008	/* IOPB length */
#define	CEVSB_WORK_QUEUE	0x000009	/* work queue number */
#define	CEVSB_ERROR		0x00000d	/* error code */
#define	CEVSB_ERR_TYPE	0xc0			/* IOPB type error */
#define	CEVSB_ERR_TO	0xc1			/* IOPB timeout error */
#define	CEVSB_ERR_TR	0x82			/* Target Reconnect, no IOPB */
#define	CEVSB_ERR_OF	0x83			/* Overflow */
#define	CEVSB_ERR_BD	0x84			/* Bad direction */
#define	CEVSB_ERR_NR	0x86			/* Non-Recoverabl Error */
#define	CESVB_ERR_PANIC	0xff			/* Board Painc!!! */
#define	CEVSB_AUXERR		0x00000e	/* cougar error code */
#define	CEVSB_SIZE		0x000010

/*
 * Configuration Status Block (CSB)
 */

#define	CSB_TYPE		0x000000	/* jaguar/cougar */
#define	COUGAR		0x4220
#define	JAGUAR		0x0000
#define	CSB_PCODE		0x000003	/* product code */
#define	CSB_PVAR		0x000009	/* product variation */
#define	CSB_FREV		0x00000d	/* firmware revision level */
#define	CSB_FDATE		0x000012	/* firmware release date */
#define	CSB_SSIZE		0x00001a	/* system memory size in KB */
#define	CSB_BSIZE		0x00001c	/* buffer memory size in KB */
#define	CSB_PFECID		0x000020	/* primary bus FEC id */
#define	CSB_SFECID		0x000021	/* secondary bus FEC id */
#define	CSB_PID			0x000022	/* primary bus id */
#define	CSB_SID			0x000023	/* secondary bus id */
#define	CSB_LPDS		0x000024	/* last primary device selected */
#define	CSB_LSDS		0x000025	/* last secondary device selected */
#define	CSB_PPS			0x000026	/* primary phase sense */
#define	CSB_SPS			0x000027	/* secondary phase sense */
#define	CSB_DBIB		0x000029	/* daughter board id */
#define	CSB_SDS			0x00002b	/* software DIP switch */
#define	CSB_FWQR		0x00002e	/* frozen work queues register */
#define	CSB_SIZE		0x000078

/*
 * IOPB Format (IOPB)
 */

#define	IOPB_CMD		0x000000	/* command code */
#define	IOPB_OPTION		0x000002	/* option word */
#define	M_OPT_HEAD_TAG		0x3000	/* head of queue command queue tag */
#define	M_OPT_ORDERED_TAG	0x2000	/* order command queue tag */
#define	M_OPT_SIMPLE_TAG	0x1000	/* simple command queue tag */
#define	M_OPT_GO_WIDE		0x0800	/* use WIDE transfers */
#define	M_OPT_DIR		0x0100	/* VME direction bit */
#define	M_OPT_SG_BLOCK		0x0008	/* scatter/gather in 512 byte blocks */
#define	M_OPT_SS		0x0004	/* Suppress synchronous transfer */
#define	M_OPT_SG		0x0002	/* scatter/gather bit */
#define	M_OPT_IE		0x0001	/* Interrupt enable */
#define	IOPB_STATUS		0x000004	/* return status word */
#define	IOPB_NVCT		0x000008	/* normal completion vector */
#define	IOPB_EVCT		0x000009	/* error completion vector */
#define	IOPB_LEVEL		0x00000a	/* interrupt level */
#define	IOPB_ADDR		0x00000e	/* address type and modifier */
#define	M_ADR_TRANS	0x0c00		/* transfer type */
#define	M_ADR_MEMT	0x0300		/* memory type */
#define	M_ADR_MOD	0x00ff		/* VME address modifier */
#define	M_ADR_SG_LINK	0x8000		/* Scatter/Gather Link bit */
#define	IOPB_BUFF		0x000010	/* buffer address */
#define	IOPB_LENGTH		0x000014	/* max transfer length */
#define	IOPB_SGTTL		0x000018	/* S/G total transfer length */
#define	IOPB_UNIT		0x00001e	/* unit address on scsi bus */
#define	M_UNIT_EXT_LUN	0xff00		/* Extended Address */
#define	M_UNIT_EXT	0x0080		/* Extended Address Enable */
#define	M_UNIT_BUS	0x0040		/* SCSI Bus Selection */
#define	M_UNIT_LUN	0x0038		/* Logical Unit Number */
#define	M_UNIT_ID	0x0007		/* SCSI Device ID */
#define	IOPB_SCSI_DATA		0x000020	/* SCSI words for passthrough */
#define	IOPB_SHORT_SIZE		0x000020
#define	IOPB_LONG_SIZE		0x000040

/*
 * Initialize Work Queue Command Format (WQCF)
 * This is a specific case of IOPB.
 */

#define	WQCF_CMD		0x000000	/* command code */
#define	M_WOPT_IWQ	0x8000		/* initialize work queue */
#define	M_WOPT_PE	0x0008		/* parity check enable */
#define	M_WOPT_FE	0x0004		/* freeze on error enable */
#define	M_WOPT_TM	0x0002		/* target mode enable */
#define	M_WOPT_AE	0x0001		/* abort enable */
#define	WQCF_OPTION		0x000002	/* option word */
#define	WQCF_STATUS		0x000004	/* return status word */
#define	WQCF_NVCT		0x000008	/* normal completion vector */
#define	WQCF_EVCT		0x000009	/* error completion vector */
#define	WQCF_ILVL		0x00000a	/* interrupt level */
#define	WQCF_WORKQ		0x00001c	/* work queue number */
#define	WQCF_WOPT		0x00001e	/* work queue options */
#define	WQCF_SLOTS		0x000020	/* # of slots in work queues */
#define	WQCF_CMDTO		0x000024	/* command timeout */

/*
 * SCSI Reset Command Format (SRCF)
 * This is a specific case of IOPB.
 */

#define	SRCF_CMD		0x000000	/* command code */
#define	SRCF_OPTION		0x000002	/* option word */
#define	SRCF_STATUS		0x000004	/* return status word */
#define	SRCF_NVCT		0x000008	/* normal completion vector */
#define	SRCF_EVCT		0x000009	/* error completion vector */
#define	SRCF_ILVL		0x00000a	/* interrupt level */
#define	SRCF_BUSID		0x00001c	/* scsi bus id to reset */

/*
 * Device Reinitialize Command Format (DRCF)
 * This is a specific case of IOPB.
 */

#define	DRCF_CMD		0x000000	/* command code */
#define	DRCF_OPTION		0x000002	/* option word */
#define	DRCF_STATUS		0x000004	/* return status word */
#define	DRCF_NVCT		0x000008	/* normal completion vector */
#define	DRCF_EVCT		0x000009	/* error completion vector */
#define	DRCF_ILVL		0x00000a	/* interrupt level */
#define	DRCF_UNIT		0x00001e	/* unit address */

/*
 * Short I/O memory layout
 */

#define	S_SHORTIO		0x000800

#define	sh_MCSB		(0)
#define	sh_MCE		(sh_MCSB + MCSB_SIZE)
#define	sh_CQE(x)	(sh_MCE + CQE_SIZE + CQE_SIZE * (x))
#define	sh_IOPB(x)	(sh_CQE(NUM_CQE) + IOPB_LONG_SIZE * (x))
#define	sh_MCE_IOPB	(sh_IOPB(NUM_IOPB))
#define	sh_CIB		(sh_MCE_IOPB + IOPB_LONG_SIZE)

#define	sh_CSS		(S_SHORTIO - CSB_SIZE)
#define	sh_RET_IOPB	(sh_CSS - IOPB_LONG_SIZE)
#define	sh_CRB		(sh_RET_IOPB - CRB_SIZE)
#define	sh_CEVSB	sh_CRB
#define	sh_HSB		(sh_CRB - HSB_SIZE)

#if (sh_CIB + CIB_SIZE > sh_HSB)
#error	invalid memory layout
#endif

#define	SELECTION_TIMEOUT		250	/* milliseconds */
#define	VME_BUS_TIMEOUT			15	/* units of 30ms */
#define	M328_INFINITE_TIMEOUT		0	/* wait forever */

/*
 * IOPB command codes
 */

#define	IOPB_PASSTHROUGH		0x20 /* SCSI passthrough commands */
#define	IOPB_PASSTHROUGH_EXT		0x21 /* SCSI passthrough extended */
#define	IOPB_RESET			0x22 /* reset bus */

#define	CNTR_DIAG			0x40 /* perform diagnostics */
#define	CNTR_INIT			0x41 /* initialize controller */
#define	CNTR_INIT_WORKQ			0x42 /* initialize work queue */
#define	CNTR_DUMP_INIT			0x43 /* dump initialization parameters */
#define	CNTR_DUMP_WORDQ			0x44 /* dump work queue parameters */
#define	CNTR_CANCEL_IOPB		0x48 /* cancel command tag */
#define	CNTR_FLUSH_WORKQ		0x49 /* flush work queue */
#define	CNTR_DEV_REINIT			0x4c /* reinitialize device */
#define	CNTR_ISSUE_ABORT		0x4e /* abort has been issued */
#define	CNTR_DOWNLOAD_FIRMWARE		0x4f /* download firmware (COUGAR) */

/*
 * Memory types
 */

#define	MEMT_16BIT	1	/* 16 bit memory type */
#define	MEMT_32BIT	2	/* 32 bit memory type */
#define	MEMT_SHIO	3	/* short I/O memory type */
#define	MEMTYPE		MEMT_32BIT

/*
 * Transfer types
 */

#define	TT_NORMAL		0	/* normal mode tranfers */
#define	TT_BLOCK		1	/* block mode tranfers */
#define	TT_DISABLE_INC_ADDR	2	/* disable incrementing addresses */
#define	TT_D64			3	/* D64 mode transfers */

/*
 * Error codes
 */

#define	MACSI_GOOD_STATUS	0x00
#define	MACSI_QUEUE_FULL	0x01
#define	MACSI_CMD_CODE_ERR	0x04
#define	MACSI_QUEUE_NUMBER_ERR	0x05

#define	RESET_BUS_STATUS	0x11	/* SCSI bus reset IOPB forced this */
#define	NO_SECONDARY_PORT	0x12	/* second SCSI bus not available */
#define	SCSI_DEVICE_IS_RESET	0x14
#define	CMD_ABORT_BY_RESET	0x15

#define	VME_BUS_ERROR		0x20
#define	VME_BUS_ACC_TIMEOUT	0x21
#define	VME_BUS_BAD_ADDR	0x23
#define	VME_BUS_BAD_MEM_TYPE	0x24
#define	VME_BUS_BAD_COUNT	0x25
#define	VME_BUS_FETCH_ERROR	0x26
#define	VME_BUS_FETCH_TIMEOUT	0x27
#define	VME_BUS_POST_ERROR	0x28
#define	VME_BUS_POST_TIMEOUT	0x29
#define	VME_BUS_BAD_FETCH_ADDR	0x2a
#define	VME_BUS_BAD_POST_ADDR	0x2b
#define	VME_BUS_SG_FETCH	0x2c
#define	VME_BUS_SG_TIMEOUT	0x2d
#define	VME_BUS_SG_COUNT	0x2e

#define	SCSI_SELECTION_TO	0x30
#define	SCSI_DISCONNECT_TIMEOUT	0x31
#define	SCSI_ABNORMAL_SEQ	0x32
#define	SCSI_DISCONNECT_ERR	0x33
#define	SCSI_XFER_EXCEPTION	0x34
#define	SCSI_PARITY_ERROR	0x35

#define	DEVICE_NO_IOPB		0x82
#define	IOPB_CTLR_EXH		0x83
#define	IOPB_DIR_ERROR		0x84
#define	COUGAR_ERROR		0x86
#define	MACSI_INCORRECT_HW	0x90
#define	MACSI_ILGL_IOPB_VAL	0x92
#define	MACSI_ILLEGAL_IMAGE	0x9c
#define	IOPB_TYPE_ERR		0xc0	/* IOPB type not 0 */
#define	IOPB_TIMEOUT		0xc1

#define	COUGAR_PANIC		0xff

#define	MACSI_INVALID_TIMEOUT	0x843

/*
 * VME addressing modes
 */

#define	ADRM_STD_S_P		0x3e	/* standard supervisory program */
#define	ADRM_STD_S_D		0x3d	/* standard supervisory data */
#define	ADRM_STD_N_P		0x3a	/* standard normal program */
#define	ADRM_STD_N_D		0x39	/* standard normal data */
#define	ADRM_SHT_S_IO		0x2d	/* short supervisory I/O */
#define	ADRM_SHT_N_IO		0x29	/* short normal I/O */
#define	ADRM_EXT_S_P		0x0e	/* extended supervisory program */
#define	ADRM_EXT_S_D		0x0d	/* extended supervisory data */
#define	ADRM_EXT_N_P		0x0a	/* extended normal program */
#define	ADRM_EXT_N_D		0x09	/* extended normal data */
#define	ADRM_EXT_S_BM		0x0f	/* extended supervisory block mode */
#define	ADRM_EXT_S_D64		0x0c	/* extended supervisory D64 mode */

#define	ADDR_MOD	((TT_NORMAL << 10) | (MEMTYPE << 8) | ADRM_EXT_S_D)
#define	BLOCK_MOD	((TT_BLOCK << 10) | (MEMTYPE << 8) | ADRM_EXT_S_BM)
#define	D64_MOD		((TT_D64 << 10) | (MEMTYPE << 8) | ADRM_EXT_S_D64)
#define	SHIO_MOD	((TT_NORMAL << 10) | (MEMT_SHIO << 8) | ADRM_SHT_N_IO)

#endif	/* _MVME328_REG_H_ */
