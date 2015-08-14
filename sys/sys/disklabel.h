/*	$OpenBSD: disklabel.h,v 1.67 2015/08/14 23:45:56 krw Exp $	*/
/*	$NetBSD: disklabel.h,v 1.41 1996/05/10 23:07:37 mark Exp $	*/

/*
 * Copyright (c) 1987, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *
 *	@(#)disklabel.h	8.2 (Berkeley) 7/10/94
 */

/*
 * Disk description table, see disktab(5)
 */
#define	_PATH_DISKTAB	"/etc/disktab"
#define	DISKTAB		"/etc/disktab"		/* deprecated */

/*
 * Each disk has a label which includes information about the hardware
 * disk geometry, filesystem partitions, and drive specific information.
 * The location of the label, as well as the number of partitions the
 * label can describe and the number of the "whole disk" (raw)
 * partition are machine dependent.
 */
#include <machine/disklabel.h>

#include <sys/uuid.h>

/*
 * The absolute maximum number of disk partitions allowed.
 * This is the maximum value of MAXPARTITIONS for which 'struct disklabel'
 * is <= DEV_BSIZE bytes long.  If MAXPARTITIONS is greater than this, beware.
 */
#define	MAXMAXPARTITIONS	22
#if MAXPARTITIONS > MAXMAXPARTITIONS
#warn beware: MAXPARTITIONS bigger than MAXMAXPARTITIONS
#endif

/*
 * Translate between device numbers and major/disk unit/disk partition.
 */
#define	DISKUNIT(dev)	(minor(dev) / MAXPARTITIONS)
#define	DISKPART(dev)	(minor(dev) % MAXPARTITIONS)
#define	RAW_PART	2	/* 'c' partition */
#define	DISKMINOR(unit, part) \
    (((unit) * MAXPARTITIONS) + (part))
#define	MAKEDISKDEV(maj, unit, part) \
    (makedev((maj), DISKMINOR((unit), (part))))
#define	DISKLABELDEV(dev) \
    (MAKEDISKDEV(major(dev), DISKUNIT(dev), RAW_PART))

#define DISKMAGIC	((u_int32_t)0x82564557)	/* The disk magic number */

#define MAXDISKSIZE	0x7fffffffffffLL	/* 47 bits of reach */

#ifndef _LOCORE
struct disklabel {
	u_int32_t d_magic;		/* the magic number */
	u_int16_t d_type;		/* drive type */
	u_int16_t d_subtype;		/* controller/d_type specific */
	char	  d_typename[16];	/* type name, e.g. "eagle" */

	/*
	 * d_packname contains the pack identifier and is returned when
	 * the disklabel is read off the disk or in-core copy.
	 * d_boot0 and d_boot1 are the (optional) names of the
	 * primary (block 0) and secondary (block 1-15) bootstraps
	 * as found in /usr/mdec.  These are returned when using
	 * getdiskbyname(3) to retrieve the values from /etc/disktab.
	 */
	union {
		char	un_d_packname[16];	/* pack identifier */
		struct {
			char *un_d_boot0;	/* primary bootstrap name */
			char *un_d_boot1;	/* secondary bootstrap name */
		} un_b;
	} d_un;
#define d_packname	d_un.un_d_packname
#define d_boot0		d_un.un_b.un_d_boot0
#define d_boot1		d_un.un_b.un_d_boot1

			/* disk geometry: */
	u_int32_t d_secsize;		/* # of bytes per sector */
	u_int32_t d_nsectors;		/* # of data sectors per track */
	u_int32_t d_ntracks;		/* # of tracks per cylinder */
	u_int32_t d_ncylinders;		/* # of data cylinders per unit */
	u_int32_t d_secpercyl;		/* # of data sectors per cylinder */
	u_int32_t d_secperunit;		/* # of data sectors (low part) */

	u_char	d_uid[8];		/* Unique label identifier. */

	/*
	 * Alternate cylinders include maintenance, replacement, configuration
	 * description areas, etc.
	 */
	u_int32_t d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	u_int16_t d_bstarth;		/* start of useable region (high part) */
	u_int16_t d_bendh;		/* size of useable region (high part) */
	u_int32_t d_bstart;		/* start of useable region */
	u_int32_t d_bend;		/* end of useable region */
	u_int32_t d_flags;		/* generic flags */
#define NDDATA 5
	u_int32_t d_drivedata[NDDATA];	/* drive-type specific information */
	u_int16_t d_secperunith;	/* # of data sectors (high part) */
	u_int16_t d_version;		/* version # (1=48 bit addressing) */
#define NSPARE 4
	u_int32_t d_spare[NSPARE];	/* reserved for future use */
	u_int32_t d_magic2;		/* the magic number (again) */
	u_int16_t d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	u_int16_t d_npartitions;	/* number of partitions in following */
	u_int32_t d_bbsize;		/* size of boot area at sn0, bytes */
	u_int32_t d_sbsize;		/* max size of fs superblock, bytes */
	struct	partition {		/* the partition table */
		u_int32_t p_size;	/* number of sectors (low part) */
		u_int32_t p_offset;	/* starting sector (low part) */
		u_int16_t p_offseth;	/* starting sector (high part) */
		u_int16_t p_sizeh;	/* number of sectors (high part) */
		u_int8_t p_fstype;	/* filesystem type, see below */
		u_int8_t p_fragblock;	/* encoded filesystem frag/block */
		u_int16_t p_cpg;	/* UFS: FS cylinders per group */
	} d_partitions[MAXPARTITIONS];	/* actually may be more */
};


struct	__partitionv0 {		/* old (v0) partition table entry */
	u_int32_t p_size;	/* number of sectors in partition */
	u_int32_t p_offset;	/* starting sector */
	u_int32_t p_fsize;	/* filesystem basic fragment size */
	u_int8_t p_fstype;	/* filesystem type, see below */
	u_int8_t p_frag;	/* filesystem fragments per block */
	union {
		u_int16_t cpg;	/* UFS: FS cylinders per group */
		u_int16_t sgs;	/* LFS: FS segment shift */
	} __partitionv0_u1;
};
#endif /* _LOCORE */


#define DISKLABELV1_FFS_FRAGBLOCK(fsize, frag) 			\
	((fsize) * (frag) == 0 ? 0 :				\
	(((ffs((fsize) * (frag)) - 13) << 3) | (ffs(frag))))

#define DISKLABELV1_FFS_BSIZE(i) ((i) == 0 ? 0 : (1 << (((i) >> 3) + 12)))
#define DISKLABELV1_FFS_FRAG(i) ((i) == 0 ? 0 : (1 << (((i) & 0x07) - 1)))
#define DISKLABELV1_FFS_FSIZE(i) (DISKLABELV1_FFS_FRAG(i) == 0 ? 0 : \
	(DISKLABELV1_FFS_BSIZE(i) / DISKLABELV1_FFS_FRAG(i)))

#define DL_GETPSIZE(p)		(((u_int64_t)(p)->p_sizeh << 32) + (p)->p_size)
#define DL_SETPSIZE(p, n)	do { \
					u_int64_t x = (n); \
					(p)->p_sizeh = x >> 32; \
					(p)->p_size = x; \
				} while (0)
#define DL_GETPOFFSET(p)	(((u_int64_t)(p)->p_offseth << 32) + (p)->p_offset)
#define DL_SETPOFFSET(p, n)	do { \
					u_int64_t x = (n); \
					(p)->p_offseth = x >> 32; \
					(p)->p_offset = x; \
				} while (0)

#define DL_GETDSIZE(d)		(((u_int64_t)(d)->d_secperunith << 32) + \
				    (d)->d_secperunit)
#define DL_SETDSIZE(d, n)	do { \
					u_int64_t x = (n); \
					(d)->d_secperunith = x >> 32; \
					(d)->d_secperunit = x; \
				} while (0)
#define DL_GETBSTART(d)		(((u_int64_t)(d)->d_bstarth << 32) + \
				    (d)->d_bstart)
#define DL_SETBSTART(d, n)	do { \
					u_int64_t x = (n); \
					(d)->d_bstarth = x >> 32; \
					(d)->d_bstart = x; \
				} while (0)
#define DL_GETBEND(d)		(((u_int64_t)(d)->d_bendh << 32) + \
				    (d)->d_bend)
#define DL_SETBEND(d, n)	do { \
					u_int64_t x = (n); \
					(d)->d_bendh = x >> 32; \
					(d)->d_bend = x; \
				} while (0)

#define DL_BLKSPERSEC(d)	((d)->d_secsize / DEV_BSIZE)
#define DL_SECTOBLK(d, n)	((n) * DL_BLKSPERSEC(d))
#define DL_BLKTOSEC(d, n)	((n) / DL_BLKSPERSEC(d))
#define DL_BLKOFFSET(d, n)	(((n) % DL_BLKSPERSEC(d)) * DEV_BSIZE)

/* d_type values: */
#define	DTYPE_SMD		1		/* SMD, XSMD; VAX hp/up */
#define	DTYPE_MSCP		2		/* MSCP */
#define	DTYPE_DEC		3		/* other DEC (rk, rl) */
#define	DTYPE_SCSI		4		/* SCSI */
#define	DTYPE_ESDI		5		/* ESDI interface */
#define	DTYPE_ST506		6		/* ST506 etc. */
#define	DTYPE_HPIB		7		/* CS/80 on HP-IB */
#define	DTYPE_HPFL		8		/* HP Fiber-link */
#define	DTYPE_FLOPPY		10		/* floppy */
#define	DTYPE_CCD		11		/* was: concatenated disk device */
#define	DTYPE_VND		12		/* vnode pseudo-disk */
#define	DTYPE_ATAPI		13		/* ATAPI */
#define DTYPE_RAID		14		/* was: RAIDframe */
#define DTYPE_RDROOT		15		/* ram disk root */

#ifdef DKTYPENAMES
static char *dktypenames[] = {
	"unknown",
	"SMD",
	"MSCP",
	"old DEC",
	"SCSI",
	"ESDI",
	"ST506",
	"HP-IB",
	"HP-FL",
	"type 9",
	"floppy",
	"ccd",			/* deprecated */
	"vnd",
	"ATAPI",
	"RAID",
	"rdroot",
	NULL
};
#define DKMAXTYPES	(sizeof(dktypenames) / sizeof(dktypenames[0]) - 1)
#endif

/*
 * Filesystem type and version.
 * Used to interpret other filesystem-specific
 * per-partition information.
 */
#define	FS_UNUSED	0		/* unused */
#define	FS_SWAP		1		/* swap */
#define	FS_V6		2		/* Sixth Edition */
#define	FS_V7		3		/* Seventh Edition */
#define	FS_SYSV		4		/* System V */
#define	FS_V71K		5		/* V7 with 1K blocks (4.1, 2.9) */
#define	FS_V8		6		/* Eighth Edition, 4K blocks */
#define	FS_BSDFFS	7		/* 4.2BSD fast file system */
#define	FS_MSDOS	8		/* MSDOS file system */
#define	FS_BSDLFS	9		/* 4.4BSD log-structured file system */
#define	FS_OTHER	10		/* in use, but unknown/unsupported */
#define	FS_HPFS		11		/* OS/2 high-performance file system */
#define	FS_ISO9660	12		/* ISO 9660, normally CD-ROM */
#define	FS_BOOT		13		/* partition contains bootstrap */
#define	FS_ADOS		14		/* AmigaDOS fast file system */
#define	FS_HFS		15		/* Macintosh HFS */
#define	FS_ADFS		16		/* Acorn Disk Filing System */
#define FS_EXT2FS	17		/* ext2fs */
#define FS_CCD		18		/* ccd component */
#define FS_RAID		19		/* RAIDframe or softraid */
#define FS_NTFS		20		/* Windows/NT file system */
#define FS_UDF		21		/* UDF (DVD) filesystem */

#ifdef DKTYPENAMES
static char *fstypenames[] = {
	"unused",
	"swap",
	"Version6",
	"Version7",
	"SystemV",
	"4.1BSD",
	"Eighth-Edition",
	"4.2BSD",
	"MSDOS",
	"4.4LFS",
	"unknown",
	"HPFS",
	"ISO9660",
	"boot",
	"ADOS",
	"HFS",
	"ADFS",
	"ext2fs",
	"ccd",
	"RAID",
	"NTFS",
	"UDF",
	NULL
};

/* Similar to the above, but used for things like the mount command. */
static char *fstypesnames[] = {
	"",		/* 0 */
	"",		/* 1 */
	"",		/* 2 */
	"",		/* 3 */
	"",		/* 4 */
	"",		/* 5 */
	"",		/* 6 */
	"ffs",		/* 7 */
	"msdos",	/* 8 */
	"lfs",		/* 9 */
	"",		/* 10 */
	"",		/* 11 */
	"cd9660",	/* 12 */
	"",		/* 13 */
	"ados",		/* 14 */
	"",		/* 15 */
	"",		/* 16 */
	"ext2fs",	/* 17 */
	"",		/* 18 */
	"",		/* 19 */
	"ntfs",		/* 20 */
	"udf",		/* 21 */
	NULL
};

#define FSMAXTYPES	(sizeof(fstypenames) / sizeof(fstypenames[0]) - 1)
#endif

/*
 * flags shared by various drives:
 */
#define		D_BADSECT	0x04		/* supports bad sector forw. */
#define		D_VENDOR	0x08		/* vendor disklabel */

/*
 * Drive data for SMD.
 */
#define	d_smdflags	d_drivedata[0]
#define		D_SSE		0x1		/* supports skip sectoring */
#define	d_mindist	d_drivedata[1]
#define	d_maxdist	d_drivedata[2]
#define	d_sdist		d_drivedata[3]

/*
 * Drive data for ST506.
 */
#define d_precompcyl	d_drivedata[0]
#define d_gap3		d_drivedata[1]		/* used only when formatting */

/*
 * Drive data for SCSI.
 */
#define	d_blind		d_drivedata[0]

#ifndef _LOCORE
/*
 * Structure used to perform a format or other raw operation, returning
 * data and/or register values.  Register identification and format
 * are device- and driver-dependent.
 */
struct format_op {
	char	*df_buf;
	int	 df_count;		/* value-result */
	daddr_t	 df_startblk;
	int	 df_reg[8];		/* result */
};

/*
 * Structure used internally to retrieve information about a partition
 * on a disk.
 */
struct partinfo {
	struct disklabel *disklab;
	struct partition *part;
};

/* GUID partition table -- located at sector 1 of some disks. */
#define	GPTSECTOR		1	/* DOS boot block relative sector # */
#define	GPTSIGNATURE		0x5452415020494645LL
				/* ASCII string "EFI PART" encoded as 64-bit */
#define	GPTREVISION		0x10000		/* GPT header version 1.0 */
#define	NGPTPARTITIONS		128
#define	GPTDOSACTIVE		0x2
#define	GPTMINHDRSIZE		92
#define	GPTMINPARTSIZE		128

/* all values in the GPT need to be little endian as per UEFI specification */
struct gpt_header {
	u_int64_t gh_sig;	/* "EFI PART" */
	u_int32_t gh_rev;	/* GPT Version 1.0: 0x00000100 */
	u_int32_t gh_size;	/* Little-Endian */
	u_int32_t gh_csum;	/* CRC32: with this field as 0 */
	u_int32_t gh_rsvd;	/* always zero */
	u_int64_t gh_lba_self;	/* LBA of this header */
	u_int64_t gh_lba_alt;	/* LBA of alternate header */
	u_int64_t gh_lba_start;	/* first usable LBA */
	u_int64_t gh_lba_end;	/* last usable LBA */
	struct uuid gh_guid;	/* disk GUID used to identify the disk */
	u_int64_t gh_part_lba;	/* starting LBA of GPT partition entries */
	u_int32_t gh_part_num;	/* # of partition entries */
	u_int32_t gh_part_size;	/* size per entry, shall be 128*(2**n)
				   with n >= 0 */
	u_int32_t gh_part_csum;	/* CRC32 checksum of all partition entries:
				 * starts at gh_part_lba and is computed over
				 * a byte length of gh_part_num*gh_part_size */
	/* the rest of the block is reserved by UEFI and must be zero */
};

struct gpt_partition {
	struct uuid gp_type;	/* partition type GUID */
	struct uuid gp_guid;	/* unique partition GUID */
	u_int64_t gp_lba_start;	/* starting LBA of this partition */
	u_int64_t gp_lba_end;	/* ending LBA of this partition, inclusive,
				   usually odd */
	u_int64_t gp_attrs;	/* attribute flags */
	u_int16_t gp_name[36];	/* partition name, utf-16le */
	/* the rest of the GPT partition entry, if any, is reserved by UEFI
	   and must be zero */
};

#define GPT_PARTSPERSEC(gh)	(DEV_BSIZE / letoh32((gh)->gh_part_size))
#define GPT_SECOFFSET(gh, n)	((gh)->gh_part_size * n)

#define GPT_UUID_UNUSED \
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
#define GPT_UUID_MSDOS \
    { 0xeb, 0xd0, 0xa0, 0xa2, 0xb9, 0xe5, 0x44, 0x33, \
      0x87, 0xc0, 0x68, 0xb6, 0xb7, 0x26, 0x99, 0xc7 }
#define GPT_UUID_EFI_SYSTEM \
    { 0xc1, 0x2a, 0x73, 0x28, 0xf8, 0x1f, 0x11, 0xd2, \
      0xba, 0x4b, 0x00, 0xa0, 0xc9, 0x3e, 0xc9, 0x3b }
#define GPT_UUID_LEGACY_MBR \
    { 0x02, 0x4d, 0xee, 0x41, 0x33, 0x37, 0x11, 0xd3, \
      0x9d, 0x69, 0x00, 0x08, 0xc7, 0x81, 0xf3, 0x9f }
#define GPT_UUID_OPENBSD \
    { 0x82, 0x4c, 0xc7, 0xa0, 0x36, 0xa8, 0x11, 0xe3, \
      0x89, 0x0a, 0x95, 0x25, 0x19, 0xad, 0x3f, 0x61 }
#define GPT_UUID_CHROMEROOTFS \
    { 0x3c, 0xb8, 0xe2, 0x02, 0x3b, 0x7e, 0x47, 0xdd, \
      0x8a, 0x3c, 0x7f, 0xf2, 0xa1, 0x3c, 0xfc, 0xec }
#define GPT_UUID_LINUX \
    { 0x0f, 0xc6, 0x3d, 0xaf, 0x84, 0x83, 0x47, 0x72, \
      0x8e, 0x79, 0x3d, 0x69, 0xd8, 0x47, 0x7d, 0xe4 }
#define GPT_UUID_LINUX_HOME \
    { 0x93, 0x3a, 0xc7, 0xe1, 0x2e, 0xb4, 0x4f, 0x13, \
      0xb8, 0x44, 0x0e, 0x14, 0xe2, 0xae, 0xf9, 0x15 }
#define GPT_UUID_LINUX_SRV \
    { 0x3b, 0x8f, 0x84, 0x25, 0x20, 0xe0, 0x4f, 0x3b, \
      0x90, 0x7f, 0x1a, 0x25, 0xa7, 0x6f, 0x98, 0xe8 }
#define GPT_UUID_FBSD_DATA \
    { 0x51, 0x6e, 0x7c, 0xb4, 0x6e, 0xcf, 0x11, 0xd6, \
      0x8f, 0xf8, 0x00, 0x02, 0x2d, 0x09, 0x71, 0x2b }
#define GPT_UUID_FBSD_UFS \
    { 0x51, 0x6e, 0x7c, 0xb6, 0x6e, 0xcf, 0x11, 0xd6, \
      0x8f, 0xf8, 0x00, 0x02, 0x2d, 0x09, 0x71, 0x2b }
#define GPT_UUID_NBSD_UFS \
    { 0x49, 0xf4, 0x8d, 0x5a, 0xb1, 0x0e, 0x11, 0xdc, \
      0xb9, 0x9b, 0x00, 0x19, 0xd1, 0x87, 0x96, 0x48 }
#define GPT_UUID_APPLE_HFS \
    { 0x48, 0x46, 0x53, 0x00, 0x00, 0x00, 0x11, 0xaa, \
      0xaa, 0x11, 0x00, 0x30, 0x65, 0x43, 0xec, 0xac }
#define GPT_UUID_APPLE_UFS \
    { 0x55, 0x46, 0x53, 0x00, 0x00, 0x00, 0x11, 0xaa, \
      0xaa, 0x11, 0x00, 0x30, 0x65, 0x43, 0xec, 0xac }

/* DOS partition table -- located at start of some disks. */
#define	DOS_LABELSECTOR 1
#define	DOSBBSECTOR	0		/* DOS boot block relative sector # */
#define	DOSPARTOFF	446
#define	DOSDISKOFF	444
#define	NDOSPART	4
#define	DOSACTIVE	0x80		/* active partition */

#define	DOSMBR_SIGNATURE	(0xaa55)
#define	DOSMBR_SIGNATURE_OFF	(0x1fe)

/* Maximum number of Extended Boot Records (EBRs) to traverse. */
#define	DOS_MAXEBR	256

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

/* Isolate the relevant bits to get sector and cylinder. */
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))

/* Known DOS partition types. */
#define	DOSPTYP_UNUSED	0x00		/* Unused partition */
#define	DOSPTYP_FAT12	0x01		/* 12-bit FAT */
#define	DOSPTYP_FAT16S	0x04		/* 16-bit FAT, less than 32M */
#define	DOSPTYP_EXTEND	0x05		/* Extended; contains sub-partitions */
#define	DOSPTYP_FAT16B	0x06		/* 16-bit FAT, more than 32M */
#define	DOSPTYP_NTFS	0x07		/* NTFS */
#define	DOSPTYP_FAT32	0x0b		/* 32-bit FAT */
#define	DOSPTYP_FAT32L	0x0c		/* 32-bit FAT, LBA-mapped */
#define	DOSPTYP_FAT16L	0x0e		/* 16-bit FAT, LBA-mapped */
#define	DOSPTYP_EXTENDL 0x0f		/* Extended, LBA-mapped; (sub-partitions) */
#define	DOSPTYP_ONTRACK	0x54
#define	DOSPTYP_LINUX	0x83		/* That other thing */
#define	DOSPTYP_FREEBSD	0xa5		/* FreeBSD partition type */
#define	DOSPTYP_OPENBSD	0xa6		/* OpenBSD partition type */
#define	DOSPTYP_NETBSD	0xa9		/* NetBSD partition type */
#define	DOSPTYP_EFI	0xee		/* EFI Protective Partition */
#define	DOSPTYP_EFISYS	0xef		/* EFI System Partition */

struct dos_mbr {
	u_int8_t		dmbr_boot[DOSPARTOFF];
	struct dos_partition	dmbr_parts[NDOSPART];
	u_int16_t		dmbr_sign;
} __packed;

#ifdef _KERNEL
void	 diskerr(struct buf *, char *, char *, int, int, struct disklabel *);
u_int	 dkcksum(struct disklabel *);
int	 initdisklabel(struct disklabel *);
int	 checkdisklabel(void *, struct disklabel *, u_int64_t, u_int64_t);
int	 setdisklabel(struct disklabel *, struct disklabel *, u_int);
int	 readdisklabel(dev_t, void (*)(struct buf *), struct disklabel *, int);
int	 writedisklabel(dev_t, void (*)(struct buf *), struct disklabel *);
int	 bounds_check_with_label(struct buf *, struct disklabel *);
int	 readdoslabel(struct buf *, void (*)(struct buf *),
	    struct disklabel *, daddr_t *, int);
#ifdef GPT
int	 readgptlabel(struct buf *, void (*)(struct buf *),
	    struct disklabel *, daddr_t *, int);
#endif
#ifdef CD9660
int iso_disklabelspoof(dev_t dev, void (*strat)(struct buf *),
	struct disklabel *lp);
#endif
#ifdef UDF
int udf_disklabelspoof(dev_t dev, void (*strat)(struct buf *),
	struct disklabel *lp);
#endif
#endif
#endif /* _LOCORE */

#if !defined(_KERNEL) && !defined(_LOCORE)

#include <sys/cdefs.h>

__BEGIN_DECLS
struct disklabel *getdiskbyname(const char *);
__END_DECLS

#endif
