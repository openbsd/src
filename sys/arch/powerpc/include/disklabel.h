/*	$NetBSD: disklabel.h,v 1.1 1996/09/30 16:34:22 ws Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_MACHINE_DISKLABEL_H_
#define	_MACHINE_DISKLABEL_H_

#define	LABELSECTOR	1		/* sector containing label */
#define	LABELOFFSET	0		/* offset of label in sector */
#define	MAXPARTITIONS	16		/* number of partitions */
#define	RAW_PART	2		/* raw partition: XX?c */

/* MBR partition table */
#define	MBRSECTOR	0		/* MBR sector number */
#define	MBRPARTOFF	446		/* Offset of MBR partition table */
#define	NMBRPART	4		/* # of partitions in MBR */
#define	MBRMAGICOFF	510		/* Offset of magic number */
#define	MBRMAGIC	0xaa55		/* Actual magic number */

struct mbr_partition {
	unsigned char	mbr_flag;	/* default boot flag */
	unsigned char	mbr_shd;	/* start head, IsN't Always Meaningful */
	unsigned char	mbr_ssect;	/* start sector, INAM */
	unsigned char	mbr_scyl;	/* start cylinder, INAM */
	unsigned char	mbr_type;	/* partition type */
	unsigned char	mbr_ehd;	/* end head, INAM */
	unsigned char	mbr_esect;	/* end sector, INAM */
	unsigned char	mbr_ecyl;	/* end cylinder, INAM */
	unsigned long	mbr_start;	/* absolute start sector number */
	unsigned long	mbr_size;	/* partition size in sectors */
};

/* Known partition types: */
#define	MBR_EXTENDED	0x05		/* Extended partition */
#define	MBR_NETBSD_LE	0xa5		/* NetBSD little endian partition */
#define	MBR_NETBSD_BE	0xa6		/* NetBSD big endian partition */
#define	MBR_NETBSD	MBR_NETBSD_BE	/* on this machine, we default to BE */

/* For compatibility reasons (mainly for fdisk): */
#define	dos_partition	mbr_partition
#define	dp_flag		mbr_flag
#define	dp_shd		mbr_shd
#define	dp_ssect	mbr_ssect
#define	dp_scyl		mbr_scyl
#define	dp_typ		mbr_type
#define	dp_ehd		mbr_ehd
#define	dp_esect	mbr_esect
#define	dp_ecyl		mbr_ecyl
#define	dp_start	mbr_start
#define	dp_size		mbr_size

#define	DOSPARTOFF	MBRPARTOFF
#define	NDOSPART	NMBRPART

#define	DOSPTYP_386BSD	MBR_NETBSD
#define DOSPTYP_OPENBSD     0xa6            /* OpenBSD partition type */

struct cpu_disklabel {
	int cd_start;		/* Offset to NetBSD partition in blocks */
};

/* Isolate the relevant bits to get sector and cylinder. */
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))

#ifdef	_KERNEL
struct disklabel;
int bounds_check_with_label __P((struct buf *bp, struct disklabel *lp, int wlabel));
#endif	/* _KERNEL */

#endif	/* _MACHINE_DISKLABEL_H_ */
