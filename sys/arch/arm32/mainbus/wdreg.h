/*	$NetBSD: wdreg.h,v 1.1 1996/01/31 23:25:18 mark Exp $	*/

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
#define wd_error	0x004	/* error register (R) */
#define	wd_precomp	0x004	/* write precompensation (W) */
#define	wd_features	0x004	/* features (W) */
#define	wd_seccnt	0x008	/* sector count (R/W) */
#define	wd_sector	0x00c	/* first sector number (R/W) */
#define	wd_cyl_lo	0x010	/* cylinder address, low byte (R/W) */
#define	wd_cyl_hi	0x014	/* cylinder address, high byte (R/W) */
#define	wd_sdh		0x018	/* sector size/drive/head (R/W) */
#define	wd_command	0x01c	/* command register (W)	*/
#define	wd_status	0x01c	/* immediate status (R)	*/

#define	wd_altsts	0x818	/* alternate fixed disk status (via 1015) (R) */
#define	wd_ctlr		0x818	/* fixed disk controller control (via 1015) (W) */
#define  WDCTL_4BIT	 0x08	/* use four head bits (wd1003) */
#define  WDCTL_RST	 0x04	/* reset the controller */
#define  WDCTL_IDS	 0x02	/* disable controller interrupts */
#define	wd_digin	0x81c	/* disk controller input (via 1015) (R) */

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


#ifdef _KERNEL
/*
 * read parameters command returns this:
 */
struct wdparams {
	/* drive info */
	short	wdp_config;		/* general configuration */
#define	WD_CFG_REMOVABLE	0x0080
#define	WD_CFG_FIXED		0x0040
	short	wdp_cylinders;		/* number of non-removable cylinders */
	char	__reserved1[2];
	short	wdp_heads;		/* number of heads */
	short	wdp_unfbytespertrk;	/* number of unformatted bytes/track */
	short	wdp_unfbytespersec;	/* number of unformatted bytes/sector */
	short	wdp_sectors;		/* number of sectors */
	char	wdp_vendor1[6];
	/* controller info */
	char	wdp_serial[20];		/* serial number */
	short	wdp_buftype;		/* buffer type */
#define	WD_BUF_SINGLEPORTSECTOR	1	 /* single port, single sector buffer */
#define	WD_BUF_DUALPORTMULTI	2	 /* dual port, multiple sector buffer */
#define	WD_BUF_DUALPORTMULTICACHE 3	 /* above plus track cache */
	short	wdp_bufsize;		/* buffer size, in 512-byte units */
	short	wdp_eccbytes;		/* ecc bytes appended */
	char	wdp_revision[8];	/* firmware revision */
	char	wdp_model[40];		/* model name */
	u_char	wdp_maxmulti;		/* maximum sectors per interrupt */
	char	wdp_vendor2[1];
	short	wdp_usedmovsd;		/* can use double word read/write? */
	char	wdp_vendor3[1];
	char	wdp_capabilities;	/* capability flags */
#define	WD_CAP_LBA	0x02
#define	WD_CAP_DMA	0x01
	char	__reserved2[2];
	char	wdp_vendor4[1];		
	char	wdp_piotiming;		/* PIO timing mode */
	char	wdp_vendor5[1];
	char	wdp_dmatiming;		/* DMA timing mode */
};
#endif /* _KERNEL */
