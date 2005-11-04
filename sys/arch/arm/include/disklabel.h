/*	$OpenBSD: disklabel.h,v 1.5 2005/11/04 13:33:59 uwe Exp $	*/
/*	$OpenBSD: disklabel.h,v 1.5 2005/11/04 13:33:59 uwe Exp $	*/
/*	$NetBSD: disklabel.h,v 1.2 2001/11/25 19:02:03 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * disklabel.h
 *
 * machine specific disk label info
 *
 * Created      : 04/10/94
 */

#ifndef _ARM_DISKLABEL_H_
#define _ARM_DISKLABEL_H_

#define LABELSECTOR	1		/* sector containing label */
#define LABELOFFSET	0		/* offset of label in sector */
#define MAXPARTITIONS	16		/* number of partitions */
#define RAW_PART	2		/* raw partition: XX?c */

#include <sys/dkbad.h>
#if 0
#include <arm/disklabel_acorn.h>
#include <sys/disklabel_mbr.h>
#endif

/* MBR partition table */
#define	DOSBBSECTOR	0		/* MBR sector number */
#define	DOSPARTOFF	446		/* Offset of MBR partition table */
#define	NDOSPART	4		/* # of partitions in MBR */
#define	DOSMAGICOFF	510		/* Offset of magic number */
#define	DOSMAGIC	0xaa55		/* Actual magic number */
#define	MBRMAGIC	DOSMAGIC
#define DOSMBR_SIGNATURE MBRMAGIC
#define DOSMBR_SIGNATURE_OFF DOSMAGICOFF
#define	DOSACTIVE	0x80


struct dos_partition {
	u_int8_t	dp_flag;	/* bootstrap flags */
	u_int8_t	dp_shd;		/* starting head */
	u_int8_t	dp_ssect;	/* starting sector */
	u_int8_t	dp_scyl;	/* starting cylinder */
	u_int8_t	dp_typ;		/* partition type (see below) */
	u_int8_t	dp_ehd;		/* end head */
	u_int8_t	dp_esect;	/* end sector */
	u_int8_t	dp_ecyl;	/* end cylinder */
	u_int32_t	dp_start;	/* absolute starting sector number */
	u_int32_t	dp_size;	/* partition size in sectors */
};

/* Known DOS partition types. */
#define	DOSPTYP_UNUSED	0x00		/* Unused partition */
#define DOSPTYP_FAT12	0x01		/* 12-bit FAT */
#define DOSPTYP_FAT16S	0x04		/* 16-bit FAT, less than 32M */
#define DOSPTYP_EXTEND	0x05		/* Extended; contains sub-partitions */
#define DOSPTYP_FAT16B	0x06		/* 16-bit FAT, more than 32M */
#define DOSPTYP_FAT32	0x0b		/* 32-bit FAT */
#define DOSPTYP_FAT32L	0x0c		/* 32-bit FAT, LBA-mapped */
#define DOSPTYP_FAT16C	0x0e		/* 16-bit FAT, CHS-mapped */
#define DOSPTYP_EXTENDL 0x0f		/* Extended, LBA-mapped; contains sub-partitions */
#define DOSPTYP_ONTRACK	0x54
#define	DOSPTYP_LINUX	0x83		/* That other thing */
#define DOSPTYP_FREEBSD	0xa5		/* FreeBSD partition type */
#define DOSPTYP_OPENBSD	0xa6		/* OpenBSD partition type */
#define DOSPTYP_NETBSD	0xa9		/* NetBSD partition type */

/* Isolate the relevant bits to get sector and cylinder. */
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))


#ifdef __ARMEL__
#define get_le(x) (*((u_int32_t *)x))
#else
static __inline u_int32_t get_le(void *p);
static __inline u_int32_t
get_le(void *p)
{
	u_int8_t *_p = (u_int8_t *)p;
	int x;
	x = _p[0];
	x |= _p[1] << 8;
	x |= _p[2] << 16;
	x |= _p[3] << 24;
	return x;
}
#endif


#define NMBRPART 4
struct cpu_disklabel {
	struct dos_partition dosparts[NMBRPART];
	struct dkbad bad;
};

#endif /* _ARM_DISKLABEL_H_ */
