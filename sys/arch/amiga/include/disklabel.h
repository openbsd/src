/*	$OpenBSD: disklabel.h,v 1.2 1996/05/02 06:44:45 niklas Exp $	*/
/*	$NetBSD: disklabel.h,v 1.6 1996/04/21 21:13:19 veego Exp $	*/

/*
 * Copyright (c) 1994 Christian E. Hopps
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
 *      This product includes software developed by Christian E. Hopps.
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

#define	LABELSECTOR	0			/* sector containing label */
#define	LABELOFFSET	64			/* offset of label in sector */
#define	MAXPARTITIONS	16			/* number of partitions */
#define	RAW_PART	2			/* raw partition: xx?c */

/*
 * describes ados Rigid Disk Blocks
 * which are used to partition a drive
 */
#define RDBNULL ((u_long)0xffffffff)

/*
 * you will find rdblock somewhere in [0, RDBMAXBLOCKS)
 */
#define RDB_MAXBLOCKS	16

struct rdblock {
	u_long id;		/* 'RDSK' */
	u_long nsumlong;	/* number of longs in check sum */
	u_long chksum;		/* simple additive with wrap checksum */
	u_long hostid;		/* scsi target of host */
	u_long nbytes;		/* size of disk blocks */
	u_long flags;
	u_long badbhead;	/* linked list of badblocks */
	u_long partbhead;	/* linked list of partblocks */
	u_long fsbhead;		/*   "     "   of fsblocks */
	u_long driveinit;
	u_long resv1[6];	/* RDBNULL */
	u_long ncylinders;	/* number of cylinders on drive */
	u_long nsectors;	/* number of sectors per track */
	u_long nheads;		/* number of tracks per cylinder */
	u_long interleave;
	u_long park;		/* only used with st506 i.e. not */
	u_long resv2[3];
	u_long wprecomp;	/* start cyl for write precomp */
	u_long reducedwrite;	/* start cyl for reduced write current */
	u_long steprate;	/* driver step rate in ?s */
	u_long resv3[5];
	u_long rdblowb;		/* lowblock of range for rdb's */
	u_long rdbhighb;	/* high block of range for rdb's */
	u_long lowcyl;		/* low cylinder of partition area */
	u_long highcyl;		/* upper cylinder of partition area */
	u_long secpercyl;	/* number of sectors per cylinder */
	u_long parkseconds;	/* zero if no park needed */
	u_long resv4[2];
	char   diskvendor[8];	/* inquiry stuff */
	char   diskproduct[16];	/* inquiry stuff */
	char   diskrevision[4];	/* inquiry stuff */
	char   contvendor[8];	/* inquiry stuff */
	char   contproduct[16];	/* inquiry stuff */
	char   contrevision[4];	/* inquiry stuff */
#if never_use_secsize
	u_long resv5[0];
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
	u_long tabsize;		/* 0: environ table size */
	u_long sizeblock;	/* 1: n long words in a block */
	u_long secorg;		/* 2: not used must be zero */
	u_long numheads;	/* 3: number of surfaces */
	u_long secperblk;	/* 4: must be 1 */
	u_long secpertrk;	/* 5: blocks per track */
	u_long resvblocks;	/* 6: reserved blocks at start */
	u_long prefac;		/* 7: must be 0 */
	u_long interleave;	/* 8: normally 1 */
	u_long lowcyl;		/* 9: low cylinder of partition */
	u_long highcyl;		/* 10: upper cylinder of partition */
	u_long numbufs;		/* 11: ados: number of buffers */
	u_long membuftype;	/* 12: ados: type of bufmem */
	u_long maxtrans;	/* 13: maxtrans the ctrlr supports */
	u_long mask;		/* 14: mask for valid address */
	u_long bootpri;		/* 15: boot priority for autoboot */
	u_long dostype;		/* 16: filesystem type */
	u_long baud;		/* 17: serial handler baud rate */
	u_long control;		/* 18: control word for fs */
	u_long bootblocks;	/* 19: blocks containing boot code */
	u_long fsize;		/* 20: file system block size */
	u_long frag;		/* 21: allowable frags per block */
	u_long cpg;		/* 22: cylinders per group */
};

struct partblock {
	u_long id;		/* 'PART' */
	u_long nsumlong;	/* number of longs in check sum */
	u_long chksum;		/* simple additive with wrap checksum */
	u_long hostid;		/* scsi target of host */
	u_long next;		/* next in chain */
	u_long flags;		/* see below */
	u_long resv1[3];
	u_char partname[32];	/* (BCPL) part name (may not be unique) */
	u_long resv2[15];
	struct ados_environ e;
#if never_use_secsize
	u_long extra[9];	/* 8 for extra added to environ */
#endif
};

#define PBF_BOOTABLE	0x1	/* partition is bootable */
#define PBF_NOMOUNT	0x2	/* partition should be mounted */

struct badblock {
	u_long id;		/* 'BADB' */
	u_long nsumlong;	/* number of longs in check sum */
	u_long chksum;		/* simple additive with wrap checksum */
	u_long hostid;		/* scsi target of host */
	u_long next;		/* next in chain */
	u_long resv;
	struct badblockent {
		u_long badblock;
		u_long goodblock;
	} badtab[0];		/* 61 for secsize == 512 */
};

struct fsblock {
	u_long id;		/* 'FSHD' */
	u_long nsumlong;	/* number of longs in check sum */
	u_long chksum;		/* simple additive with wrap checksum */
	u_long hostid;		/* scsi target of host */
	u_long next;		/* next in chain */
	u_long flags;
	u_long resv1[2];
	u_long dostype;		/* this is a file system for this type */
	u_long version;		/* version of this fs */
	u_long patchflags;	/* describes which functions to replace */
	u_long type;		/* zero */
	u_long task;		/* zero */
	u_long lock;		/* zero */
	u_long handler;		/* zero */
	u_long stacksize;	/* to use when loading handler */
	u_long priority;	/* to run the fs at. */
	u_long startup;		/* zero */
	u_long lsegblocks;	/* linked list of lsegblocks of fs code */
	u_long globalvec;	/* bcpl vector not used mostly */
#if never_use_secsize
	u_long resv2[44];
#endif
};

struct lsegblock {
	u_long id;		/* 'LSEG' */
	u_long nsumlong;	/* number of longs in check sum */
	u_long chksum;		/* simple additive with wrap checksum */
	u_long hostid;		/* scsi target of host */
	u_long next;		/* next in chain */
	u_long loaddata[0];	/* load segment data, 123 for secsize == 512 */
};

#define RDBLOCK_ID	0x5244534b	/* 'RDSK' */
#define PARTBLOCK_ID	0x50415254	/* 'PART' */
#define BADBLOCK_ID	0x42414442	/* 'BADB' */
#define FSBLOCK_ID	0x46534844	/* 'FSHD' */
#define LSEGBLOCK_ID	0x4c534547	/* 'LSEG' */

struct cpu_disklabel {
	u_long rdblock;			/* may be RDBNULL which invalidates */
	u_long pblist[MAXPARTITIONS];	/* partblock number (RDB list order) */
	int pbindex[MAXPARTITIONS];	/* index of pblock (partition order) */
	int  valid;			/* essential that this is valid */
};

#endif /* _MACHINE_DISKLABEL_H_ */
