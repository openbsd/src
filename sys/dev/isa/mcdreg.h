/*	$OpenBSD: mcdreg.h,v 1.4 1999/01/23 01:13:12 espie Exp $	*/
/*	$NetBSD: mcdreg.h,v 1.8 1997/04/04 18:59:37 christos Exp $	*/

/*
 * Copyright 1993 by Holger Veit (data part)
 * Copyright 1993 by Brian Moore (audio part)
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
 *	This software was developed by Holger Veit and Brian Moore
 *      for use with "386BSD" and similar operating systems.
 *    "Similar operating systems" includes mainly non-profit oriented
 *    systems for research and education, including but not restricted to
 *    "NetBSD", "FreeBSD", "Mach" (by CMU).
 * 4. Neither the name of the developer(s) nor the name "386BSD"
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This file contains definitions for some cdrom control commands
 * and status codes. This info was "inherited" from the DOS MTMCDE.SYS
 * driver, and is thus not complete (and may even be wrong). Some day
 * the manufacturer or anyone else might provide better documentation,
 * so this file (and the driver) will then have a better quality.
 */

typedef unsigned char	bcd_t;
#define	M_msf(msf)	msf[0]
#define	S_msf(msf)	msf[1]
#define	F_msf(msf)	msf[2]

#define	MCD_COMMAND	0
#define	MCD_STATUS	0
#define	MCD_RDATA	0
#define	MCD_RESET	1
#define	MCD_XFER	1
#define	MCD_CTL2	2 /* XXX Is this right? */
#define	MCD_CONFIG	3
#define MCD_NPORT	4

#define	MCD_MASK_DMA	0x07	/* bits 2-0 = DMA channel */
#define	MCD_MASK_IRQ	0x70	/* bits 6-4 = INT number */
				/* 001 = int 2,9 */
				/* 010 = int 3 */
				/* 011 = int 5 */
				/* 100 = int 10 */
				/* 101 = int 11 */

/* Status bits */
#define	MCD_ST_DOOROPEN		0x80
#define	MCD_ST_DSKIN		0x40
#define	MCD_ST_DSKCHNG		0x20
#define	MCD_ST_SPINNING		0x10
#define	MCD_ST_AUDIODISK	0x08	/* audio disk is in */
#define	MCD_ST_READERR		0x04
#define	MCD_ST_AUDIOBSY		0x02	/* audio disk is playing */
#define	MCD_ST_CMDCHECK		0x01	/* command error */

/* Xfer bits */
#define	MCD_XF_STATUSUNAVAIL	0x04
#define	MCD_XF_DATAUNAVAIL	0x02

/* Modes */
#define	MCD_MD_TESTMODE		0x80	/* 0 = DATALENGTH is valid */
#define	MCD_MD_DATALENGTH	0x40	/* 1 = read ECC data also */
#define	MCD_MD_ECCMODE		0x20	/* 1 = disable secondary ECC */
#define	MCD_MD_SPINDOWN		0x08	/* 1 = spin down */
#define	MCD_MD_READTOC		0x04	/* 1 = read TOC on GETQCHN */
#define	MCD_MD_PLAYAUDIO	0x01	/* 1 = play audio through headphones */

#define	MCD_MD_RAW		(MCD_MD_PLAYAUDIO|MCD_MD_ECCMODE|MCD_MD_DATALENGTH)
#define	MCD_MD_COOKED		(MCD_MD_PLAYAUDIO)
#define	MCD_MD_TOC		(MCD_MD_PLAYAUDIO|MCD_MD_READTOC)
#define	MCD_MD_SLEEP		(MCD_MD_PLAYAUDIO|MCD_MD_SPINDOWN)

#define	MCD_BLKSIZE_RAW		sizeof(struct mcd_rawsector)
#define	MCD_BLKSIZE_COOKED	2048

/* Lock states */
#define	MCD_LK_UNLOCK		0x00
#define	MCD_LK_LOCK		0x01
#define	MCD_LK_TEST		0x02

/* Config commands */
#define	MCD_CF_IRQENABLE	0x10
#define	MCD_CF_DMATIMEOUT	0x08
#define	MCD_CF_READUPC		0x04
#define	MCD_CF_DMAENABLE	0x02
#define	MCD_CF_BLOCKSIZE	0x01

/* UPC subcommands */
#define	MCD_UPC_DISABLE		0x00
#define	MCD_UPC_ENABLE		0x01

/* commands known by the controller */
#define	MCD_CMDRESET		0x00
#define	MCD_CMDGETVOLINFO	0x10	/* gets mcd_volinfo */
#define	MCD_CMDGETDISKINFO	0x11	/* gets mcd_disk */
#define	MCD_CMDGETQCHN		0x20	/* gets mcd_qchninfo */
#define	MCD_CMDGETSENSE		0x30	/* gets sense info */
#define	MCD_CMDGETSTAT		0x40	/* gets a byte of status */
#define	MCD_CMDSETMODE		0x50	/* set transmission mode, needs byte */
#define	MCD_CMDSTOPAUDIO	0x70
#define	MCD_CMDSTOPAUDIOTIME	0x80
#define	MCD_CMDGETVOLUME	0x8E	/* gets mcd_volume */
#define	MCD_CMDCONFIGDRIVE	0x90
#define	MCD_CMDSETDRIVEMODE	0xa0	/* set drive mode */
#define	MCD_CMDSETVOLUME	0xae	/* sets mcd_volume */
#define	MCD_CMDREAD1		0xb0	/* read n sectors */
#define	MCD_CMDREADSINGLESPEED	0xc0	/* read (single speed) */
#define	MCD_CMDREADDOUBLESPEED	0xc1	/* read (double speed) */
#define	MCD_CMDGETDRIVEMODE	0xc2	/* get drive mode */
#define	MCD_CMDREAD3		0xc3	/* ? */
#define	MCD_CMDSETINTERLEAVE	0xc8	/* set interleave for read */
#define	MCD_CMDCONTINFO		0xdc	/* get controller info */
#define	MCD_CMDSTOP		0xf0	/* stop everything */
#define	MCD_CMDEJECTDISK	0xf6
#define	MCD_CMDCLOSETRAY	0xf8
#define	MCD_CMDSETLOCK		0xfe	/* needs byte */

union mcd_qchninfo {
	struct {
		u_char	control:4;
		u_char	addr_type:4;
		u_char	trk_no;
		u_char	idx_no;
		bcd_t	track_size[3];
		u_char	:8;
		bcd_t	absolute_pos[3];
	} toc;
	struct {
		u_char	control:4;
		u_char	addr_type:4;
		u_char	trk_no;
		u_char	idx_no;
		bcd_t	relative_pos[3];
		u_char	:8;
		bcd_t	absolute_pos[3];
	} current;
	struct {
		u_char	control:4;
		u_char	addr_type:4;
		u_char	upccode[7];
		u_char	junk[2];
	} upc;
} __attribute__((packed));

struct mcd_volinfo {
	bcd_t	trk_low;
	bcd_t	trk_high;
	bcd_t	vol_msf[3];
	bcd_t	trk1_msf[3];
} __attribute__((packed));

struct mcd_result {
	u_char	length;
	union {
		struct {
			u_char	data[1];
		} raw;
		struct {
			u_char	code;
			u_char	version;
		} continfo;
		union mcd_qchninfo qchninfo;
		struct mcd_volinfo volinfo;
	} data;
} __attribute__((packed));

struct mcd_command {
	u_char	opcode;
	u_char	length;
	union {
		struct {
			u_char	data[1];
		} raw;
		struct {
			bcd_t	start_msf[3];
			bcd_t	reserved[3];
		} seek;
		struct {
			bcd_t	start_msf[3];
			bcd_t	length[3];
		} read;
		struct {
			bcd_t	start_msf[3];
			bcd_t	end_msf[3];
		} play;
		struct {
			u_char	mode;
		} datamode;
		struct {
			u_char	time;
		} hold;
		struct {
			u_char	mode;
		} drivemode;
		struct {
			u_char	mode;
		} lockmode;
		struct {
			u_char	subcommand;
			u_char	data1, data2;
		} config;
	} data;
} __attribute__((packed));

struct mcd_mbox {
	struct mcd_command cmd;
	struct mcd_result res;
} __attribute__((packed));

struct mcd_volume {
	u_char	v0l;
	u_char	v0rs;
	u_char	v0r;
	u_char	v0ls;
} __attribute__((packed));

struct mcd_rawsector {
	u_char	sync1[12];
	u_char	header[4];
	u_char	subheader1[4];
	u_char	subheader2[4];
	u_char	data[MCD_BLKSIZE_COOKED];
	u_char	ecc_bits[280];
} __attribute__((packed));
