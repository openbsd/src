/*	$OpenBSD: disklabel.h,v 1.5 1996/10/06 19:59:56 pefo Exp $	*/
/*	$NetBSD: disklabel.h,v 1.2 1995/01/18 06:37:55 mellon Exp $	*/

/*
 * Copyright (c) 1994 Christopher G. Demetriou
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
 *      This product includes software developed by Christopher G. Demetriou.
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

#ifndef _MACHINE_DISKLABEL_H_
#define _MACHINE_DISKLABEL_H_

#define	LABELSECTOR	1			/* sector containing label */
#define	LABELOFFSET	0			/* offset of label in sector */
#define	MAXPARTITIONS	16			/* number of partitions */
#define	PARTITIONSHIFT	4			/* log2 */
#define	PARTITIONMASK	0xf			
#define	RAW_PART	2			/* raw partition: xx?c */

/* DOS partition table -- used when the system is booted from a dos
 * partition. This is the case on NT systems.
 */
#define	DOSBBSECTOR	0		/* DOS boot block relative sector # */
#define	DOSPARTOFF	446
#define	NDOSPART	4

struct dos_partition {
	unsigned char	dp_flag;	/* bootstrap flags */
	unsigned char	dp_shd;		/* starting head */
	unsigned char	dp_ssect;	/* starting sector */
	unsigned char	dp_scyl;	/* starting cylinder */
	unsigned char	dp_typ;		/* partition type (see below) */
	unsigned char	dp_ehd;		/* end head */
	unsigned char	dp_esect;	/* end sector */
	unsigned char	dp_ecyl;	/* end cylinder */
	unsigned long	dp_start;	/* absolute starting sector number */
	unsigned long	dp_size;	/* partition size in sectors */
} dos_partitions[NDOSPART];

/* Known DOS partition types. */
#define DOSPTYP_386BSD	0xa5		/* 386BSD partition type */
#define DOSPTYP_NETBSD	DOSPTYP_386BSD	/* NetBSD partition type (XXX) */
#define DOSPTYP_OPENBSD	0xa6		/* OpenBSD partition type */
#define DOSPTYP_FAT12   0x1
#define DOSPTYP_FAT16S  0x4
#define DOSPTYP_FAT16B  0x6
#define DOSPTYP_FAT16C  0xe

#include <sys/dkbad.h>
struct cpu_disklabel {
	struct dos_partition dosparts[NDOSPART];
	struct dkbad bad;
};

/* Isolate the relevant bits to get sector and cylinder. */
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))

#ifdef _KERNEL
struct disklabel;
int	bounds_check_with_label __P((struct buf *, struct disklabel *, int));
#endif

#endif /* _MACHINE_DISKLABEL_H_ */
