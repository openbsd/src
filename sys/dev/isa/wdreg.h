/*	$OpenBSD: wdreg.h,v 1.8 1998/08/08 23:01:12 downsj Exp $	*/
/*	$NetBSD: wdreg.h,v 1.13 1995/03/29 21:56:46 briggs Exp $	*/

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
#define	wd_data		0x000	/* data register (R/W - 16 bits) */
#define wd_error	0x001	/* error register (R) */
#define	wd_precomp	0x001	/* write precompensation (W) */
#define	wd_features	0x001	/* features (W) */
#define	wd_seccnt	0x002	/* sector count (R/W) */
#define wd_ireason	0x002   /* interrupt reason (R/W) (for atapi) */
#define	wd_sector	0x003	/* first sector number (R/W) */
#define	wd_cyl_lo	0x004	/* cylinder address, low byte (R/W) */
#define	wd_cyl_hi	0x005	/* cylinder address, high byte (R/W) */
#define	wd_sdh		0x006	/* sector size/drive/head (R/W) */
#define	wd_command	0x007	/* command register (W)	*/
#define	wd_status	0x007	/* immediate status (R)	*/

/*
 * Control register definiations.
 */
#define WDCTL_OFFSET	0x206	/* offset from the first set. */

#define	wd_altsts	0x000	/* alternate fixed disk status (via 1015) (R) */
#define	wd_ctlr		0x000	/* fixed disk controller control (via 1015) (W) */
#define  WDCTL_4BIT	 0x08	/* use four head bits (wd1003) */
#define  WDCTL_RST	 0x04	/* reset the controller */
#define  WDCTL_IDS	 0x02	/* disable controller interrupts */
#if 0
#define	wd_digin	0x001	/* disk controller input (via 1015) (R) */
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
#define	WDCE_UNC	0x40	/* uncorrectable data error */
#define	WDCE_MC		0x20	/* media changed */
#define	WDCE_IDNF	0x10	/* id not found */
#define	WDCE_ABRT	0x08	/* aborted command */
#define	WDCE_MCR	0x04	/* media change requested */
#define	WDCE_TK0NF	0x02	/* track 0 not found */
#define	WDCE_AMNF	0x01	/* address mark not found */
#define WDERR_BITS	"\020\010bbk\007unc\006mc\005idnf\004mcr\003abrt\002tk0nf\001amnf"

/*
 * Commands for Disk Controller.
 */
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

#define	WDCC_IDENTIFY	0xec	/* read parameters from controller */
#define	WDCC_CACHEC	0xef	/* cache control */

#define	WDSD_IBM	0xa0	/* forced to 512 byte sector, ecc */
#define	WDSD_CHS	0x00	/* cylinder/head/sector addressing */
#define	WDSD_LBA	0x40	/* logical block addressing */

/* Commands for ATAPI devices */
#define ATAPI_CHECK_POWER_MODE      0xe5 
#define ATAPI_EXEC_DRIVE_DIAGS      0x90
#define ATAPI_IDLE_IMMEDIATE        0xe1
#define ATAPI_NOP           0x00
#define ATAPI_PACKET_COMMAND        0xa0 
#define ATAPI_IDENTIFY_DEVICE       0xa1 
#define ATAPI_SOFT_RESET        0x08
#define ATAPI_SET_FEATURES      0xef
#define ATAPI_SLEEP         0xe6
#define ATAPI_STANDBY_IMMEDIATE     0xe0

/* ireason */
#define WDCI_CMD         0x01    /* command(1) or data(0) */
#define WDCI_IN          0x02    /* transfer to(1) or from(0) the host */
#define WDCI_RELEASE     0x04    /* bus released until completion */

#define PHASE_CMDOUT    (WDCS_DRQ | WDCI_CMD)  
#define PHASE_DATAIN    (WDCS_DRQ | WDCI_IN)
#define PHASE_DATAOUT   WDCS_DRQ
#define PHASE_COMPLETED (WDCI_IN | WDCI_CMD)
#define PHASE_ABORTED   0


#if	defined(_KERNEL) && !defined(_LOCORE)
/*
 * read parameters command returns this:
 */
struct wdparams {
	u_int16_t wdp_config;		/* general configuration */
#define	WD_CFG_REMOVABLE	0x0080
#define	WD_CFG_FIXED		0x0040
	u_int16_t wdp_cylinders;	/* number of non-removable cylinders */
	u_int8_t  __reserved1[2];
	u_int16_t wdp_heads;		/* number of heads */
	u_int16_t wdp_unfbytespertrk;	/* number of unformatted bytes/track */
	u_int16_t wdp_unfbytespersec;	/* number of unformatted bytes/sector */
	u_int16_t wdp_sectors;		/* number of sectors */
	u_int8_t  wdp_vendor1[6];
	u_int8_t  wdp_serial[20];	/* serial number */
	u_int16_t wdp_buftype;		/* buffer type */
#define	WD_BUF_SINGLEPORTSECTOR	1	/* single port, single sector buffer */
#define	WD_BUF_DUALPORTMULTI	2	/* dual port, multiple sector buffer */
#define	WD_BUF_DUALPORTMULTICACHE 3	/* above plus track cache */
	u_int16_t wdp_bufsize;		/* buffer size, in 512-byte units */
	u_int16_t wdp_eccbytes;		/* ecc bytes appended */
	u_int8_t  wdp_revision[8];	/* firmware revision */
	u_int8_t  wdp_model[40];	/* model name */
	u_int8_t  wdp_maxmulti;		/* maximum sectors per interrupt */
	u_int8_t  wdp_vendor2[1];
	u_int16_t wdp_usedmovsd;	/* can use double word read/write? */
	u_int8_t  wdp_vendor3[1];
	u_int8_t  wdp_capabilities;	/* capability flags */
#define	WD_CAP_DMA	0x01
#define	WD_CAP_LBA	0x02
#define WD_CAP_IORDYSW	0x04
#define WD_CAP_IODRYSUP	0x08
	u_int8_t  __reserved2[2];
	u_int8_t  wdp_vendor4[1];		
	u_int8_t  wdp_piotiming;	/* PIO timing mode */
	u_int8_t  wdp_vendor5[1];
	u_int8_t  wdp_dmatiming;	/* DMA timing mode */
	u_int16_t wdp_capvalid;		/* valid capabilities */
	u_int16_t wdp_curcyls;		/* logical cylinders */
	u_int16_t wdp_curheads;		/* logical heads */
	u_int16_t wdp_cursectors;	/* logical sectors per track */
	u_int32_t wdp_curcapacity;	/* logical total sectors on drive */
	u_int8_t  wdp_curmulti;		/* current multiple sector count */
	u_int8_t  wdp_valmulti;		/* multiple sector is valid */
#define WD_CAP_MULTI	0x01
	u_int32_t wdp_lbacapacity;	/* total number of sectors */
	u_int16_t wdp_dma1word;		/* single-word dma info */
	u_int16_t wdp_dmamword;		/* multiple-word dma info */
	u_int16_t wdp_eidepiomode;	/* EIDE PIO mode */
#define WD_CAP_PIO3	0x01
#define WD_CAP_PIO4	0x02
	u_int16_t wdp_eidedmamin;	/* min mword dma cycle time (ns) */
	u_int16_t wdp_eidedmatime;	/* rec'd mword dma cycle time (ns) */
	u_int16_t wdp_eidepiotime;	/* min cycle time (ns), no IORDY  */
	u_int16_t wdp_eidepioiordy;	/* min cycle time (ns), with IORDY */
};
#endif /* _KERNEL && !_LOCORE*/
