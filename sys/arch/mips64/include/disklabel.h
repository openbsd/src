/*	$OpenBSD: disklabel.h,v 1.2 2004/08/23 14:24:56 pefo Exp $	*/
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

enum disklabel_tag { DLT_ALPHA, DLT_I386, DLT_AMIGA, DLT_HPPA, DLT_SGI };

/*
 * What disklabels are we probing for, and in which order?
 */
#ifndef LABELPROBES
#define LABELPROBES	DLT_ALPHA, DLT_I386, DLT_AMIGA, DLT_HPPA, DLT_SGI
#endif

#define	ALPHA_LABELSECTOR	0		/* sector containing label */
#define	ALPHA_LABELOFFSET	64		/* offset of label in sector */
#define	I386_LABELSECTOR	1		/* sector containing label */
#define	I386_LABELOFFSET	0		/* offset of label in sector */
#define	AMIGA_LABELSECTOR	0		/* sector containing label */
#define	AMIGA_LABELOFFSET	64		/* offset of label in sector */
#define	HPPA_LABELSECTOR	1		/* sector containing label */
#define	HPPA_LABELOFFSET	0		/* offset of label in sector */
#define	SGI_LABELSECTOR		1		/* sector containing label */
#define	SGI_LABELOFFSET		0		/* offset of label in sector */

#define LABELSECTOR		SGI_LABELSECTOR
#define LABELOFFSET		SGI_LABELOFFSET

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
#define DOSPTYP_ONTRACK	0x54
#define	DOSPTYP_LINUX	0x83		/* That other thing */
#define DOSPTYP_FREEBSD	0xa5		/* FreeBSD partition type */
#define DOSPTYP_OPENBSD	0xa6		/* OpenBSD partition type */
#define DOSPTYP_NETBSD	0xa9		/* NetBSD partition type */

/* Isolate the relevant bits to get sector and cylinder. */
#define	DPSECT(s)	((s) & 0x3f)
#define	DPCYL(c, s)	((c) + (((s) & 0xc0) << 2))

/*
 * describes ados Rigid Disk Blocks
 * which are used to partition a drive
 */
#define RDBNULL ((u_int32_t)0xffffffff)

/*
 * you will find rdblock somewhere in [0, RDBMAXBLOCKS)
 */
#define RDB_MAXBLOCKS	16

struct rdblock {
	u_int32_t id;		/* 'RDSK' */
	u_int32_t nsumlong;	/* number of longs in check sum */
	u_int32_t chksum;	/* simple additive with wrap checksum */
	u_int32_t hostid;	/* scsi target of host */
	u_int32_t nbytes;	/* size of disk blocks */
	u_int32_t flags;
	u_int32_t badbhead;	/* linked list of badblocks */
	u_int32_t partbhead;	/* linked list of partblocks */
	u_int32_t fsbhead;	/*   "     "   of fsblocks */
	u_int32_t driveinit;
	u_int32_t resv1[6];	/* RDBNULL */
	u_int32_t ncylinders;	/* number of cylinders on drive */
	u_int32_t nsectors;	/* number of sectors per track */
	u_int32_t nheads;	/* number of tracks per cylinder */
	u_int32_t interleave;
	u_int32_t park;		/* only used with st506 i.e. not */
	u_int32_t resv2[3];
	u_int32_t wprecomp;	/* start cyl for write precomp */
	u_int32_t reducedwrite;	/* start cyl for reduced write current */
	u_int32_t steprate;	/* driver step rate in ?s */
	u_int32_t resv3[5];
	u_int32_t rdblowb;	/* lowblock of range for rdb's */
	u_int32_t rdbhighb;	/* high block of range for rdb's */
	u_int32_t lowcyl;	/* low cylinder of partition area */
	u_int32_t highcyl;	/* upper cylinder of partition area */
	u_int32_t secpercyl;	/* number of sectors per cylinder */
	u_int32_t parkseconds;	/* zero if no park needed */
	u_int32_t resv4[2];
	char   diskvendor[8];	/* inquiry stuff */
	char   diskproduct[16];	/* inquiry stuff */
	char   diskrevision[4];	/* inquiry stuff */
	char   contvendor[8];	/* inquiry stuff */
	char   contproduct[16];	/* inquiry stuff */
	char   contrevision[4];	/* inquiry stuff */
#if never_use_secsize
	u_int32_t resv5[0];
#endif
};


#define RDBF_LAST	0x1	/* last drive available */
#define RDBF_LASTLUN	0x2	/* last LUN available */
#define RDBF_LASTUNIT	0x4	/* last target available */
#define RDBF_NORESELECT	0x8	/* do not use reselect */
#define RDBF_DISKID	0x10	/* disk id is valid ?? */
#define RDBF_CTRLID	0x20	/* ctrl id is valid ?? */
#define RDBF_SYNC	0x40	/* drive supports SCSI synchronous mode */
	
struct ados_environ {
	u_int32_t tabsize;	/* 0: environ table size */
	u_int32_t sizeblock;	/* 1: n long words in a block */
	u_int32_t secorg;	/* 2: not used must be zero */
	u_int32_t numheads;	/* 3: number of surfaces */
	u_int32_t secperblk;	/* 4: must be 1 */
	u_int32_t secpertrk;	/* 5: blocks per track */
	u_int32_t resvblocks;	/* 6: reserved blocks at start */
	u_int32_t prefac;	/* 7: must be 0 */
	u_int32_t interleave;	/* 8: normally 1 */
	u_int32_t lowcyl;	/* 9: low cylinder of partition */
	u_int32_t highcyl;	/* 10: upper cylinder of partition */
	u_int32_t numbufs;	/* 11: ados: number of buffers */
	u_int32_t membuftype;	/* 12: ados: type of bufmem */
	u_int32_t maxtrans;	/* 13: maxtrans the ctrlr supports */
	u_int32_t mask;		/* 14: mask for valid address */
	u_int32_t bootpri;	/* 15: boot priority for autoboot */
	u_int32_t dostype;	/* 16: filesystem type */
	u_int32_t baud;		/* 17: serial handler baud rate */
	u_int32_t control;	/* 18: control word for fs */
	u_int32_t bootblocks;	/* 19: blocks containing boot code */
	u_int32_t fsize;	/* 20: file system block size */
	u_int32_t frag;		/* 21: allowable frags per block */
	u_int32_t cpg;		/* 22: cylinders per group */
};

struct partblock {
	u_int32_t id;		/* 'PART' */
	u_int32_t nsumlong;	/* number of longs in check sum */
	u_int32_t chksum;	/* simple additive with wrap checksum */
	u_int32_t hostid;	/* scsi target of host */
	u_int32_t next;		/* next in chain */
	u_int32_t flags;	/* see below */
	u_int32_t resv1[3];
	u_char partname[32];	/* (BCPL) part name (may not be unique) */
	u_int32_t resv2[15];
	struct ados_environ e;
#if never_use_secsize
	u_int32_t extra[9];	/* 8 for extra added to environ */
#endif
};

#define PBF_BOOTABLE	0x1	/* partition is bootable */
#define PBF_NOMOUNT	0x2	/* partition should be mounted */

struct badblock {
	u_int32_t id;		/* 'BADB' */
	u_int32_t nsumlong;	/* number of longs in check sum */
	u_int32_t chksum;	/* simple additive with wrap checksum */
	u_int32_t hostid;	/* scsi target of host */
	u_int32_t next;		/* next in chain */
	u_int32_t resv;
	struct badblockent {
		u_int32_t badblock;
		u_int32_t goodblock;
	} badtab[0];		/* 61 for secsize == 512 */
};

struct fsblock {
	u_int32_t id;		/* 'FSHD' */
	u_int32_t nsumlong;	/* number of longs in check sum */
	u_int32_t chksum;	/* simple additive with wrap checksum */
	u_int32_t hostid;	/* scsi target of host */
	u_int32_t next;		/* next in chain */
	u_int32_t flags;
	u_int32_t resv1[2];
	u_int32_t dostype;	/* this is a file system for this type */
	u_int32_t version;	/* version of this fs */
	u_int32_t patchflags;	/* describes which functions to replace */
	u_int32_t type;		/* zero */
	u_int32_t task;		/* zero */
	u_int32_t lock;		/* zero */
	u_int32_t handler;	/* zero */
	u_int32_t stacksize;	/* to use when loading handler */
	u_int32_t priority;	/* to run the fs at. */
	u_int32_t startup;	/* zero */
	u_int32_t lsegblocks;	/* linked list of lsegblocks of fs code */
	u_int32_t globalvec;	/* bcpl vector not used mostly */
#if never_use_secsize
	u_int32_t resv2[44];
#endif
};

struct lsegblock {
	u_int32_t id;		/* 'LSEG' */
	u_int32_t nsumlong;	/* number of longs in check sum */
	u_int32_t chksum;	/* simple additive with wrap checksum */
	u_int32_t hostid;	/* scsi target of host */
	u_int32_t next;		/* next in chain */
	u_int32_t loaddata[0];	/* load segment data, 123 for secsize == 512 */
};

#define RDBLOCK_ID	0x5244534b	/* 'RDSK' */
#define PARTBLOCK_ID	0x50415254	/* 'PART' */
#define BADBLOCK_ID	0x42414442	/* 'BADB' */
#define FSBLOCK_ID	0x46534844	/* 'FSHD' */
#define LSEGBLOCK_ID	0x4c534547	/* 'LSEG' */

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

/* SGI */
struct devparms {
        u_int8_t        dp_skew;
        u_int8_t        dp_gap1;
        u_int8_t        dp_gap2;
        u_int8_t        dp_spares_cyl;
        u_int16_t       dp_cyls;
        u_int16_t       dp_shd0;
        u_int16_t       dp_trks0;
        u_int8_t        dp_ctq_depth;
        u_int8_t        dp_cylshi;
        u_int16_t       dp_unused;
        u_int16_t       dp_secs;
        u_int16_t       dp_secbytes;
        u_int16_t       dp_interleave;
        u_int32_t       dp_flags;
        u_int32_t       dp_datarate;
        u_int32_t       dp_nretries;
        u_int32_t       dp_mspw;
        u_int16_t       dp_xgap1;
        u_int16_t       dp_xsync;
        u_int16_t       dp_xrdly;
        u_int16_t       dp_xgap2;
        u_int16_t       dp_xrgate;
        u_int16_t       dp_xwcont;
} __attribute__((__packed__));

struct sgilabel {
#define SGILABEL_MAGIC  0xbe5a941
	u_int32_t       magic;
	int16_t         root;
	int16_t         swap;
	char            bootfile[16];
	struct devparms dp;
	struct {
		char	name[8];
		int32_t	block;
		int32_t	bytes;
	} voldir[15];
	struct {
		int32_t	blocks;
		int32_t	first;
		int32_t	type;
	} partitions[MAXPARTITIONS];
	int32_t         checksum;
	int32_t         _pad;
} __attribute__((__packed__));

#define SGI_PTYPE_VOLHDR        0
#define SGI_PTYPE_RAW           3
#define SGI_PTYPE_BSD           4
#define SGI_PTYPE_VOLUME        6
#define SGI_PTYPE_EFS           7
#define SGI_PTYPE_LVOL          8
#define SGI_PTYPE_RLVOL         9
#define SGI_PTYPE_XFS           10
#define SGI_PTYPE_XFSLOG        11
#define SGI_PTYPE_XLV           12
#define SGI_PTYPE_XVM           13


#include <sys/dkbad.h>
struct cpu_disklabel {
	enum disklabel_tag labeltag;
	int labelsector;
	union {
		struct {
		} _alpha;
		struct {
			struct dos_partition dosparts[NDOSPART];
			struct dkbad bad;
		} _i386;
		struct {
			u_int32_t rdblock;		/* RDBNULL -> inval. */
			u_int32_t pblist[MAXPARTITIONS];/* pblock number */
			int pbindex[MAXPARTITIONS];	/* index of pblock */
			int valid;			/* valid? */
		} _amiga;
		struct {
			struct lifvol lifvol;
			struct lifdir lifdir[LIF_NUMDIR];
			struct hpux_label hplabel;
		} _hppa;
	} u;
};

#define DKBAD(x) ((x)->u._i386.bad)

#endif /* _MACHINE_DISKLABEL_H_ */
