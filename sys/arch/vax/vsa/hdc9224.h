/*	$NetBSD: hdc9224.h,v 1.1 1996/07/20 18:55:12 ragge Exp $ */
/*	$OpenBSD: hdc9224.h,v 1.3 1998/05/11 07:34:51 niklas Exp $ */

/*
 * Copyright (c) 1996 Ludd, University of Lule}, Sweden.
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
 *	This product includes software developed at Ludd, University of 
 *	Lule}, Sweden and its contributors.
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



struct hdc9224_DKCreg {
	unsigned char dkc_reg;	/* Disk Register Data Access Port (rw)*/
	unsigned char fill[3];	/* bytes are longword aligned */
	unsigned char dkc_cmd;	/* Disk Controller Command Port (wo) */
#define dkc_stat dkc_cmd	/* Interrupt Status Port (ro) */
};

/*
 * definition of some commands (constant bits only, incomplete!)
 */
#define DKC_CMD_RESET		0x00	/* terminate non-data-transfer cmds */
#define DKC_CMD_DRDESELECT	0x01	/* done when no drive is in use */
#define DKC_CMD_SETREGPTR	0x40	/* logically or-ed with reg-number */
#define DKC_CMD_DRSELECT	0x20
#define DKC_CMD_DRSEL_HDD	0x24	/* select HDD, or-ed with unit-numb. */
#define DKC_CMD_DRSEL_RX33	0x28	/* or-ed with unit-number of RX33 */
#define DKC_CMD_DRSEL_RX50	0x2C	/* or-ed with unit-number of RX50 */
#define DKC_CMD_RESTORE		0x02
#define DKC_CMD_STEP		0x04
#define DKC_CMD_STEPIN_FDD	0x04	/* one step inward for floppy */
#define DKC_CMD_STEPOUT_FDD	0x06	/* one step outward (toward cyl #0) */
#define DKC_CMD_POLLDRIVE	0x10
#define DKC_CMD_SEEKREADID	0x50
#define DKC_CMD_FORMATTRACK	0x60
#define DKC_CMD_READTRACK	0x5A
#define DKC_CMD_READPHYSICAL	0x58
#define DKC_CMD_READLOGICAL	0x5C
#define DKC_CMD_READ_HDD	0x5D	/* read-logical, bypass=0, xfer=1 */
#define DKC_CMD_READ_RX33	0x5D	/* ??? */
#define DKC_CMD_WRITEPHYSICAL	0x80
#define DKC_CMD_WRITELOGICAL	0xC0
#define DKC_CMD_WRITE_HDD	0xA0	/* bypass=0, ddmark=0 */
#define DKC_CMD_WRITE_RX33	0xA1	/* precompensation differs... */
#define DKC_CMD_WRITE_RX50	0xA4

/*
 * Definition of bits in the DKC_STAT register
 */
#define DKC_ST_INTPEND	(1<<7)		/* interrupt pending */
#define DKC_ST_DMAREQ	(1<<6)		/* DMA request */
#define DKC_ST_DONE	(1<<5)		/* command done */
#define DKC_ST_TERMCOD	(3<<3)		/* termination code (see below) */
#define DKC_ST_RDYCHNG	(1<<2)		/* ready change */
#define DKC_ST_OVRUN	(1<<1)		/* overrun/underrun */
#define DKC_ST_BADSECT	(1<<0)		/* bad sector */

/*
 * Definition of the termination codes
 */
#define DKC_TC_SUCCESS	(0<<3)		/* Successful completion */
#define DKC_TC_RDIDERR	(1<<3)		/* Error in READ-ID sequence */
#define DKC_TC_VRFYERR	(2<<3)		/* Error in VERIFY sequence */
#define DKC_TC_DATAERR	(3<<3)		/* Error in DATA-TRANSFER seq. */

/*
 * Definitions of delays neccessary for floppy-operation
 */
#define DKC_DELAY_MOTOR		500	/* allow 500 ms to reach speed */
#define DKC_DELAY_SELECT	 70	/* 70 ms for data-recovery-circuit */
#define DKC_DELAY_POSITION	 59	/* 59 ms for RX33, 100 ms for RX50 */
#define DKC_DELAY_HEADSET	 18	/* 18 ms when changing head-number */

/*
 * The HDC9224 has 11/15(?) internal registers which are accessible via
 * the Disk-Register-Data-Access-Port DKC_REG
 */
struct hdc9224_UDCreg { /* internal disk controller registers */
	u_char udc_dma7;	/*  0: DMA adress bits	0 -  7 */
	u_char udc_dma15;	/*  1: DMA adress bits	8 - 15 */
	u_char udc_dma23;	/*  2: DMA adress bits 16 - 23 */
	u_char udc_dsect;	/*  3: desired/starting sector number */
#define udc_csect udc_dsect	/*     current sector number */
	u_char udc_dhead;	/*  4: cyl-bits 8-10, desired head number */
#define udc_chead udc_dhead	/*     current head number */
	u_char udc_dcyl;	/*  5: desired cylinder number */
#define udc_ccyl udc_dcyl	/*     current cylinder number */
	u_char udc_scnt;	/*  6: sector count register */
	u_char udc_rtcnt;	/*  7: retry count register */
	u_char udc_mode;	/*  8: operation mode/chip status */
#define udc_cstat udc_mode	/*     chip status register */
	u_char udc_term;	/*  9: termination conditions/drive status */
#define udc_dstat udc_term	/*     drive status register */
	u_char udc_data;	/* 10: data */
};

/*
 * Definition of bits in the Current-Head register
 */
#define UDC_CH_BADSECT	(1<<7)	/* indicates a bad sector (if bypass=0) */
#define UDC_CH_CYLBITS	(0x70)	/* bits 10-8 of current cylinder number */
#define UDC_CH_HEADNO	(0x0F)	/* current head number */

/*
 * Definition of bits in the Retry-Count register
 */
#define UDC_RC_RTRYCNT	(0xF0)	/* 1's compl. in read-log, 0 all others */
#define UDC_RC_RXDISAB	(1<<3)	/* must/should be 0 for normal operation */
#define UDC_RC_INVRDY	(1<<2)	/* polarity of floppy-status, important! */
#define UDC_RC_MOTOR	(1<<1)	/* turn on floppy-motor, no effect on HDD */
#define UDC_RC_LOSPEED	(1<<0)	/* floppy-speed select, RX33: 0, RX50: 1 */

#define UDC_RC_HDD_READ	 0xF2	/* 0x72 ??? */
#define UDC_RC_HDD_WRT	 0xF2	/* 0xF0 ??? */
#define UDC_RC_RX33READ	 0x76	/* enable retries when reading floppies */
#define UDC_RC_RX33WRT	 0xF6
#define UDC_RC_RX50READ	 0x77	/* enable retries when reading floppies */
#define UDC_RC_RX50WRT	 0xF7

/*
 * Definition of bits in the Operating-Mode register
 */
#define UDC_MD_HDMODE	(1<<7)	/* must be 1 for all FDD and HDD */
#define UDC_MD_CHKCOD	(3<<5)	/* error-check: FDD/CRC: 0, HDD/ECC: 1 */
#define UDC_MD_DENS	(1<<4)	/* density select, must be 0 */
#define UDC_MD_UNUSED	(1<<3)	/* bit 3 is not used and must be 0 */
#define UDC_MD_SRATE	(7<<0)	/* seek step rate */

#define UDC_MD_HDD	 0xC0
#define UDC_MD_RX33	 0x82
#define UDC_MD_RX50	 0x81

/*
 * Definition of bits in the Chip-Status register
 */
#define UDC_CS_RETREQ	(1<<7)	/* retry required */
#define UDC_CS_ECCATT	(1<<6)	/* error correction attempted */
#define UDC_CS_ECCERR	(1<<5)	/* ECC/CRC error */
#define UDC_CS_DELDATA	(1<<4)	/* deleted data mark */
#define UDC_CS_SYNCERR	(1<<3)	/* synchronization error */
#define UDC_CS_COMPERR	(1<<2)	/* compare error */
#define UDC_CS_PRESDRV	(0x3)	/* present drive selected */

/*
 * Definition of bits in the Termination-Conditions register
 */
#define UDC_TC_CRCPRE	(1<<7)	/* CRC register preset, must be 1 */
#define UDC_TC_UNUSED	(1<<6)	/* bit 6 is not used and must be 0 */
#define UDC_TC_INTDONE	(1<<5)	/* interrupt on done */
#define UDC_TC_TDELDAT	(1<<4)	/* terminate on deleted data */
#define UDC_TC_TDSTAT3	(1<<3)	/* terminate on drive status 3 change */
#define UDC_TC_TWPROT	(1<<2)	/* terminate on write-protect (FDD only) */
#define UDC_TC_INTRDCH	(1<<1)	/* interrupt on ready change (FDD only) */
#define UDC_TC_TWRFLT	(1<<0)	/* interrupt on write-fault (HDD only) */

#define UDC_TC_HDD	 0xA5	/* 0xB5 ??? */
#define UDC_TC_FDD	 0xA0	/* 0xAA ??? 0xB4 ??? */

/*
 * Definition of bits in the Disk-Status register
 */
#define UDC_DS_SELACK	(1<<7)	/* select acknowledge (harddisk only!) */
#define UDC_DS_INDEX	(1<<6)	/* index point */
#define UDC_DS_SKCOM	(1<<5)	/* seek complete */
#define UDC_DS_TRK00	(1<<4)	/* track 0 */
#define UDC_DS_DSTAT3	(1<<3)	/* drive status 3 (MBZ) */
#define UDC_DS_WRPROT	(1<<2)	/* write protect (floppy only!) */
#define UDC_DS_READY	(1<<1)	/* drive ready bit */
#define UDC_DS_WRFAULT	(1<<0)	/* write fault */
