/*	$NetBSD: disklabel.h,v 1.1.1.1 1996/01/07 21:54:16 leo Exp $	*/

/*
 * Copyright (c) 1995 Leo Weppelman.
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
 *      This product includes software developed by Leo Weppelman.
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

#ifndef DISKLABEL_H
#define DISKLABEL_H

/*
 * On a volume, exclusively used by NetBSD, the boot block starts at
 * sector 0. To allow shared use of a volume between two or more OS's
 * the vendor specific AHDI format is supported. In this case the boot
 * block is located at the start of an AHDI partition. In any case the
 * size of the boot block is 8KB, the disk label is at offset 7KB.
 */
#define LABELSECTOR	0		/* `natural' start of boot block   */
#define LABELOFFSET	(7 * 1024)	/* offset of disk label in bytes,
					   relative to start of boot block */
#define	BBSIZE		(8 * 1024)	/* size of boot block in bytes     */
#define MAXPARTITIONS	16		/* max. # of NetBSD partitions     */
#define RAW_PART	2		/* xx?c is raw partition	   */

#define NO_BOOT_BLOCK	((u_int) -1)
#define MAXAUXROOTS	60		/* max. # of auxilary root sectors */

/***************************************************************************/

#define	AHDI_BSIZE	512		/* AHDI blocksize		*/
#define	AHDI_BBLOCK	0		/* AHDI bootblock (root sector)	*/
#define AHDI_MAXROOTS	(MAXAUXROOTS)	/* max. # of AHDI rootsectors	*/
#define	AHDI_MAXPARTS	(AHDI_MAXROOTS+3) /* max. # of AHDI partitions	*/

/*
 * Various `well known' AHDI partition identifiers.
 */
#define AHDI_MKPID(x,y,z)	(   ((u_int32_t)(x) << 16)	\
				  | ((u_int32_t)(y) << 8)	\
				  | ((u_int32_t)(z))		\
				)
#define	AHDI_PID_XGM	AHDI_MKPID('X','G','M')
#define	AHDI_PID_GEM	AHDI_MKPID('G','E','M')
#define	AHDI_PID_BGM	AHDI_MKPID('B','G','M')
#define	AHDI_PID_RAW	AHDI_MKPID('R','A','W')
#define	AHDI_PID_SWP	AHDI_MKPID('S','W','P')
#define	AHDI_PID_NBD	AHDI_MKPID('N','B','D')
#define	AHDI_PID_NBR	AHDI_MKPID('N','B','R')
#define	AHDI_PID_NBS	AHDI_MKPID('N','B','S')
#define	AHDI_PID_NBU	AHDI_MKPID('N','B','U')

/*
 * Format of AHDI boot block.
 */
#define	AHDI_MAXRPD	4		/* max. # of partition descriptors */
					/* in the AHDI bootblock (aka root)*/
#define	AHDI_MAXARPD	2		/* max. # of partition descriptors */
					/* in an AHDI auxilary root sector */

struct ahdi_part {
	u_int8_t	ap_flg;		/* bit 0 is in-use flag            */
	u_int8_t	ap_id[3];	/* id: GEM, BGM, XGM, UNX, MIX     */
	u_int32_t	ap_offs;	/* block where partition starts    */
	u_int32_t	ap_size;	/* partition size in blocks        */
#define	ap_end	ap_size /* in the in-core copy, store end instead of size  */
};

struct ahdi_root {
	u_int8_t	 ar_fill[0x1c2];/* filler, can be boot code        */
	u_int32_t	 ar_hdsize;	/* size of entire volume in blocks */
	struct ahdi_part ar_parts[AHDI_MAXRPD]; /* root partition table    */
	u_int32_t	 ar_bslst;	/* start of bad-sector list        */
	u_int32_t	 ar_bslsize;	/* # of blocks in bad-sector list  */
	u_int16_t	 ar_cksum;
};

/***************************************************************************/

#define DISKMAGIC	((u_int32_t)0x82564557)	/* The disk magic number */

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
	u_int32_t d_secperunit;		/* # of data sectors per unit */

	/*
	 * Spares (bad sector replacements) below are not counted in
	 * d_nsectors or d_secpercyl.  Spare sectors are assumed to
	 * be physical sectors which occupy space at the end of each
	 * track and/or cylinder.
	 */
	u_int16_t d_sparespertrack;	/* # of spare sectors per track */
	u_int16_t d_sparespercyl;	/* # of spare sectors per cylinder */
	/*
	 * Alternate cylinders include maintenance, replacement, configuration
	 * description areas, etc.
	 */
	u_int32_t d_acylinders;		/* # of alt. cylinders per unit */

			/* hardware characteristics: */
	/*
	 * d_interleave, d_trackskew and d_cylskew describe perturbations
	 * in the media format used to compensate for a slow controller.
	 * Interleave is physical sector interleave, set up by the
	 * formatter or controller when formatting.  When interleaving is
	 * in use, logically adjacent sectors are not physically
	 * contiguous, but instead are separated by some number of
	 * sectors.  It is specified as the ratio of physical sectors
	 * traversed per logical sector.  Thus an interleave of 1:1
	 * implies contiguous layout, while 2:1 implies that logical
	 * sector 0 is separated by one sector from logical sector 1.
	 * d_trackskew is the offset of sector 0 on track N relative to
	 * sector 0 on track N-1 on the same cylinder.  Finally, d_cylskew
	 * is the offset of sector 0 on cylinder N relative to sector 0
	 * on cylinder N-1.
	 */
	u_int16_t d_rpm;		/* rotational speed */
	u_int16_t d_interleave;		/* hardware sector interleave */
	u_int16_t d_trackskew;		/* sector 0 skew, per track */
	u_int16_t d_cylskew;		/* sector 0 skew, per cylinder */
	u_int32_t d_headswitch;		/* head switch time, usec */
	u_int32_t d_trkseek;		/* track-to-track seek, usec */
	u_int32_t d_flags;		/* generic flags */
#define NDDATA 5
	u_int32_t d_drivedata[NDDATA];	/* drive-type specific information */
#define NSPARE 5
	u_int32_t d_spare[NSPARE];	/* reserved for future use */
	u_int32_t d_magic2;		/* the magic number (again) */
	u_int16_t d_checksum;		/* xor of data incl. partitions */

			/* filesystem and partition information: */
	u_int16_t d_npartitions;	/* number of partitions in following */
	u_int32_t d_bbsize;		/* size of boot area at sn0, bytes */
	u_int32_t d_sbsize;		/* max size of fs superblock, bytes */
	struct	partition {		/* the partition table */
		u_int32_t p_size;	/* number of sectors in partition */
		u_int32_t p_offset;	/* starting sector */
		u_int32_t p_fsize;	/* filesystem basic fragment size */
		u_int8_t p_fstype;	/* filesystem type, see below */
		u_int8_t p_frag;	/* filesystem fragments per block */
		union {
			u_int16_t cpg;	/* UFS: FS cylinders per group */
			u_int16_t sgs;	/* LFS: FS segment shift */
		} __partition_u1;
#define	p_cpg	__partition_u1.cpg
#define	p_sgs	__partition_u1.sgs
	} d_partitions[MAXPARTITIONS];	/* actually may be more */
};

#endif /* DISKLABEL_H */
