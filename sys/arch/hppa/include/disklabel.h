/*	$OpenBSD: disklabel.h,v 1.17 2007/06/07 02:55:12 krw Exp $	*/
/*	$NetBSD: disklabel.h,v 1.1 1995/02/13 23:07:34 cgd Exp $	*/

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

#define LABELSECTOR		1
#define LABELOFFSET		0

#define	MAXPARTITIONS		16		/* number of partitions */
#define	RAW_PART		2		/* raw partition: xx?c */

/* DOS partition table -- located in boot block */
#define	DOSBBSECTOR	0		/* DOS boot block relative sector # */
#define	DOSPARTOFF	446
#define DOSACTIVE	0x80
#define	NDOSPART	4
#define DOSMBR_SIGNATURE 0xaa55
#define DOSMBR_SIGNATURE_OFF 0x1fe

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
#define DOSPTYP_FAT16L	0x0e		/* 16-bit FAT, LBA-mapped */
#define DOSPTYP_EXTENDL 0x0f		/* Extended, LBA-mapped; contains sub-partitions */
#define DOSPTYP_ONTRACK	0x54
#define	DOSPTYP_LINUX	0x83		/* That other thing */
#define DOSPTYP_FREEBSD	0xa5		/* FreeBSD partition type */
#define DOSPTYP_OPENBSD	0xa6		/* OpenBSD partition type */
#define DOSPTYP_NETBSD	0xa9		/* NetBSD partition type */

/* Isolate the relevant bits to get sector and cylinder. */
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))

/*
 * volume header for "LIF" format volumes
 */
struct	lifvol {
	short	vol_id;
	char	vol_label[6];
	u_int	vol_addr;
	short	vol_oct;
	short	vol_dummy;
	u_int	vol_dirsize;
	short	vol_version;
	short	vol_zero;
	u_int	vol_number;
	u_int	vol_lastvol;
	u_int	vol_length;
	char	vol_toc[6];
	char	vol_dummy1[198];

	u_int	ipl_addr;
	u_int	ipl_size;
	u_int	ipl_entry;

	u_int	vol_dummy2;
};

struct	lifdir {
	char	dir_name[10];
	u_short	dir_type;
	u_int	dir_addr;
	u_int	dir_length;
	char	dir_toc[6];
	short	dir_flag;
	u_int	dir_implement;
};

struct lif_load {
	int address;
	int count;
};

#define	HPUX_MAGIC	0x8b7f6a3c
#define	HPUX_MAXPART	16
struct hpux_label {
	int32_t		hl_magic1;
	u_int32_t	hl_magic;
	int32_t		hl_version;
	struct {
		int32_t	hlp_blah[2];
		int32_t	hlp_start;
		int32_t	hlp_length;
	}		hl_parts[HPUX_MAXPART];
	u_int8_t	hl_flags[HPUX_MAXPART];
#define	HPUX_PART_ROOT	0x10
#define	HPUX_PART_SWAP	0x14
#define	HPUX_PART_BOOT	0x32
	int32_t		hl_blah[3*16];
	u_int16_t	hl_boot;
	u_int16_t	hl_reserved;
	int32_t		hl_magic2;
};

#define LIF_VOL_ID	-32768
#define LIF_VOL_OCT	4096
#define LIF_DIR_SWAP	0x5243
#define LIF_DIR_HPLBL	0xa271
#define	LIF_DIR_FS	0xcd38
#define	LIF_DIR_IOMAP	0xcd60
#define	LIF_DIR_HPUX	0xcd80
#define	LIF_DIR_ISL	0xce00
#define	LIF_DIR_PAD	0xcffe
#define	LIF_DIR_AUTO	0xcfff
#define	LIF_DIR_EST	0xd001
#define	LIF_DIR_TYPE	0xe942

#define LIF_DIR_FLAG	0x8001	/* dont ask me! */
#define	LIF_SECTSIZE	256

#define LIF_NUMDIR	16

#define LIF_VOLSTART	0
#define LIF_VOLSIZE	sizeof(struct lifvol)
#define LIF_DIRSTART	2048
#define LIF_DIRSIZE	(LIF_NUMDIR * sizeof(struct lifdir))
#define LIF_FILESTART	8192

#define	btolifs(b)	(((b) + (LIF_SECTSIZE - 1)) / LIF_SECTSIZE)
#define	lifstob(s)	((s) * LIF_SECTSIZE) 
#define	lifstodb(s)	((s) * LIF_SECTSIZE / DEV_BSIZE)

struct cpu_disklabel {
	int labelsector;
	union {
		struct {
			struct lifvol lifvol;
			struct lifdir lifdir[LIF_NUMDIR];
			struct hpux_label hplabel;
		} _hppa;
	} u;
};

#endif /* _MACHINE_DISKLABEL_H_ */
