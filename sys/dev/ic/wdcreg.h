/*      $OpenBSD: wdcreg.h,v 1.1 1999/07/18 21:25:16 csapuntz Exp $     */
/*	$NetBSD: wdcreg.h,v 1.22 1999/03/07 14:02:54 bouyer Exp $	*/

/*-
 * Copyright (c) 1991 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)wdreg.h	7.1 (Berkeley) 5/9/91
 */

/*
 * Disk Controller register definitions.
 */

/* offsets of registers in the 'regular' register region */
#define	wd_data		0	/* data register (R/W - 16 bits) */
#define	wd_error	1	/* error register (R) */
#define	wd_precomp	1	/* write precompensation (W) */
#define	wd_features	1	/* features (W), same as wd_precomp */
#define	wd_seccnt	2	/* sector count (R/W) */
#define	wd_ireason	2	/* interrupt reason (R/W) (for atapi) */
#define	wd_sector	3	/* first sector number (R/W) */
#define	wd_cyl_lo	4	/* cylinder address, low byte (R/W) */
#define	wd_cyl_hi	5	/* cylinder address, high byte (R/W) */
#define	wd_sdh		6	/* sector size/drive/head (R/W) */
#define	wd_command	7	/* command register (W)	*/
#define	wd_status	7	/* immediate status (R)	*/

/* offsets of registers in the auxiliary register region */
#define	wd_aux_altsts	0	/* alternate fixed disk status (R) */
#define	wd_aux_ctlr	0	/* fixed disk controller control (W) */
#define  WDCTL_4BIT	 0x08	/* use four head bits (wd1003) */
#define  WDCTL_RST	 0x04	/* reset the controller */
#define  WDCTL_IDS	 0x02	/* disable controller interrupts */
#if 0 /* NOT MAPPED; fd uses this register on PCs */
#define	wd_digin	1	/* disk controller input (R) */
#endif

/*
 * Status bits.
 */
#define	WDCS_BSY	0x80	/* busy */
#define	WDCS_DRDY	0x40	/* drive ready */
#define	WDCS_DWF	0x20	/* drive write fault */
#define	WDCS_DSC	0x10	/* drive seek complete */
#define	WDCS_DRQ	0x08	/* data request */
#define	WDCS_CORR	0x04	/* corrected data */
#define	WDCS_IDX	0x02	/* index */
#define	WDCS_ERR	0x01	/* error */
#define WDCS_BITS	"\020\010bsy\007drdy\006dwf\005dsc\004drq\003corr\002idx\001err"

/*
 * Error bits.
 */
#define	WDCE_BBK	0x80	/* bad block detected */
#define	WDCE_CRC	0x80	/* CRC error (Ultra-DMA only) */
#define	WDCE_UNC	0x40	/* uncorrectable data error */
#define	WDCE_MC		0x20	/* media changed */
#define	WDCE_IDNF	0x10	/* id not found */
#define	WDCE_MCR	0x08	/* media change requested */
#define	WDCE_ABRT	0x04	/* aborted command */
#define	WDCE_TK0NF	0x02	/* track 0 not found */
#define	WDCE_AMNF	0x01	/* address mark not found */

/*
 * Commands for Disk Controller.
 */
#define WDCC_NOP	0x00	/* NOP - Always fail with "aborted command" */
#define	WDCC_RECAL	0x10	/* disk restore code -- resets cntlr */

#define	WDCC_READ	0x20	/* disk read code */
#define	WDCC_WRITE	0x30	/* disk write code */
#define	 WDCC__LONG	 0x02	 /* modifier -- access ecc bytes */
#define	 WDCC__NORETRY	 0x01	 /* modifier -- no retrys */

#define	WDCC_FORMAT	0x50	/* disk format code */
#define	WDCC_DIAGNOSE	0x90	/* controller diagnostic */
#define	WDCC_IDP	0x91	/* initialize drive parameters */

#define	WDCC_READMULTI	0xc4	/* read multiple */
#define	WDCC_WRITEMULTI	0xc5	/* write multiple */
#define	WDCC_SETMULTI	0xc6	/* set multiple mode */

#define	WDCC_READDMA	0xc8	/* read with DMA */
#define	WDCC_WRITEDMA	0xca	/* write with DMA */

#define	WDCC_ACKMC	0xdb	/* acknowledge media change */
#define	WDCC_LOCK	0xde	/* lock drawer */
#define	WDCC_UNLOCK	0xdf	/* unlock drawer */

#define	WDCC_FLUSHCACHE	0xe7	/* Flush cache */
#define	WDCC_IDENTIFY	0xec	/* read parameters from controller */
#define	SET_FEATURES	0xef	/* set features */

#define WDCC_IDLE	0xe3	/* set idle timer & enter idle mode */
#define WDCC_IDLE_IMMED	0xe1	/* enter idle mode */
#define WDCC_SLEEP	0xe6	/* enter sleep mode */
#define WDCC_STANDBY	0xe2	/* set standby timer & enter standby mode */
#define WDCC_STANDBY_IMMED 0xe0	/* enter standby mode */
#define WDCC_CHECK_PWR	0xe5	/* check power mode */

/* Subcommands for SET_FEATURES (features register ) */
#define WDSF_EN_WR_CACHE	0x02
#define WDSF_SET_MODE    	0x03
#define WDSF_REASSIGN_EN	0x04
#define WDSF_RETRY_DS		0x33
#define WDSF_SET_CACHE_SGMT	0x54
#define WDSF_READAHEAD_DS	0x55
#define WDSF_POD_DS		0x66
#define WDSF_ECC_DS		0x77
#define WDSF_WRITE_CACHE_DS	0x82
#define WDSF_REASSIGN_DS	0x84
#define WDSF_ECC_EN		0x88
#define WDSF_RETRY_EN		0x99
#define WDSF_SET_CURRENT	0x9A
#define WDSF_READAHEAD_EN	0xAA
#define WDSF_PREFETCH_SET	0xAB
#define WDSF_POD_EN             0xCC

/* parameters uploaded to device/heads register */
#define	WDSD_IBM	0xa0	/* forced to 512 byte sector, ecc */
#define	WDSD_CHS	0x00	/* cylinder/head/sector addressing */
#define	WDSD_LBA	0x40	/* logical block addressing */

/* Commands for ATAPI devices */
#define ATAPI_CHECK_POWER_MODE	0xe5 
#define ATAPI_EXEC_DRIVE_DIAGS	0x90
#define ATAPI_IDLE_IMMEDIATE	0xe1
#define ATAPI_NOP		0x00
#define ATAPI_PKT_CMD		0xa0 
#define ATAPI_IDENTIFY_DEVICE	0xa1 
#define ATAPI_SOFT_RESET	0x08
#define ATAPI_SLEEP		0xe6
#define ATAPI_STANDBY_IMMEDIATE	0xe0

/* Bytes used by ATAPI_PACKET_COMMAND ( feature register) */
#define ATAPI_PKT_CMD_FTRE_DMA 0x01
#define ATAPI_PKT_CMD_FTRE_OVL 0x02

/* ireason */
#define WDCI_CMD         0x01    /* command(1) or data(0) */
#define WDCI_IN          0x02    /* transfer to(1) or from(0) the host */
#define WDCI_RELEASE     0x04    /* bus released until completion */

#define PHASE_CMDOUT    (WDCS_DRQ | WDCI_CMD)  
#define PHASE_DATAIN    (WDCS_DRQ | WDCI_IN)
#define PHASE_DATAOUT   WDCS_DRQ
#define PHASE_COMPLETED (WDCI_IN | WDCI_CMD)
#define PHASE_ABORTED   0

