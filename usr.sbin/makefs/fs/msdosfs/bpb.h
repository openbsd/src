/*	$OpenBSD: bpb.h,v 1.2 2016/10/16 20:26:56 natano Exp $	*/
/*	$NetBSD: bpb.h,v 1.8 2016/01/22 22:53:36 dholland Exp $	*/

/*
 * Written by Paul Popelka (paulp@uts.amdahl.com)
 *
 * You can do anything you want with this software, just don't say you wrote
 * it, and don't remove this notice.
 *
 * This software is provided "as is".
 *
 * The author supplies this software to be publicly redistributed on the
 * understanding that the author is not responsible for the correct
 * functioning of this software in any circumstances and is not liable for
 * any damages caused by this software.
 *
 * October 1992
 */

#ifndef _MSDOSFS_BPB_H_
#define _MSDOSFS_BPB_H_

/*
 * BIOS Parameter Block (BPB) for DOS 3.3
 */
struct bpb33 {
	uint16_t	bpbBytesPerSec;	/* bytes per sector */
	uint8_t		bpbSecPerClust;	/* sectors per cluster */
	uint16_t	bpbResSectors;	/* number of reserved sectors */
	uint8_t		bpbFATs;	/* number of FATs */
	uint16_t	bpbRootDirEnts;	/* number of root directory entries */
	uint16_t	bpbSectors;	/* total number of sectors */
	uint8_t		bpbMedia;	/* media descriptor */
	uint16_t	bpbFATsecs;	/* number of sectors per FAT */
	uint16_t	bpbSecPerTrack;	/* sectors per track */
	uint16_t	bpbHeads;	/* number of heads */
	uint16_t	bpbHiddenSecs;	/* number of hidden sectors */
};

/*
 * BPB for DOS 5.0 The difference is bpbHiddenSecs is a short for DOS 3.3,
 * and bpbHugeSectors is not in the 3.3 bpb.
 */
struct bpb50 {
	uint16_t	bpbBytesPerSec;	/* bytes per sector */
	uint8_t		bpbSecPerClust;	/* sectors per cluster */
	uint16_t	bpbResSectors;	/* number of reserved sectors */
	uint8_t		bpbFATs;	/* number of FATs */
	uint16_t	bpbRootDirEnts;	/* number of root directory entries */
	uint16_t	bpbSectors;	/* total number of sectors */
	uint8_t		bpbMedia;	/* media descriptor */
	uint16_t	bpbFATsecs;	/* number of sectors per FAT */
	uint16_t	bpbSecPerTrack;	/* sectors per track */
	uint16_t	bpbHeads;	/* number of heads */
	uint32_t	bpbHiddenSecs;	/* # of hidden sectors */
	uint32_t	bpbHugeSectors;	/* # of sectors if bpbSectors == 0 */
};

/*
 * BPB for DOS 7.10 (FAT32).  This one has a few extensions to bpb50.
 */
struct bpb710 {
	uint16_t	bpbBytesPerSec;	/* bytes per sector */
	uint8_t		bpbSecPerClust;	/* sectors per cluster */
	uint16_t	bpbResSectors;	/* number of reserved sectors */
	uint8_t		bpbFATs;	/* number of FATs */
	uint16_t	bpbRootDirEnts;	/* number of root directory entries */
	uint16_t	bpbSectors;	/* total number of sectors */
	uint8_t		bpbMedia;	/* media descriptor */
	uint16_t	bpbFATsecs;	/* number of sectors per FAT */
	uint16_t	bpbSecPerTrack;	/* sectors per track */
	uint16_t	bpbHeads;	/* number of heads */
	uint32_t	bpbHiddenSecs;	/* # of hidden sectors */
	uint32_t	bpbHugeSectors;	/* # of sectors if bpbSectors == 0 */
	uint32_t	bpbBigFATsecs;	/* like bpbFATsecs for FAT32 */
	uint16_t	bpbExtFlags;	/* extended flags: */
#define	FATNUM		0xf		/* mask for numbering active FAT */
#define	FATMIRROR	0x80		/* FAT is mirrored (like it always was) */
	uint16_t	bpbFSVers;	/* filesystem version */
#define	FSVERS		0		/* currently only 0 is understood */
	uint32_t	bpbRootClust;	/* start cluster for root directory */
	uint16_t	bpbFSInfo;	/* filesystem info structure sector */
	uint16_t	bpbBackup;	/* backup boot sector */
	uint8_t		bpbReserved[12]; /* Reserved for future expansion */
};

/*
 * The following structures represent how the bpb's look on disk.  shorts
 * and longs are just character arrays of the appropriate length.  This is
 * because the compiler forces shorts and longs to align on word or
 * halfword boundaries.
 *
 * XXX The little-endian code here assumes that the processor can access
 * 16-bit and 32-bit quantities on byte boundaries.  If this is not true,
 * use the macros for the big-endian case.
 */
#include <sys/endian.h>
#if (BYTE_ORDER == LITTLE_ENDIAN) && !defined(__STRICT_ALIGNMENT)
#define	getushort(x)	*((u_int16_t *)(x))
#define	getulong(x)	*((u_int32_t *)(x))
#define	putushort(p, v)	(*((u_int16_t *)(p)) = (v))
#define	putulong(p, v)	(*((u_int32_t *)(p)) = (v))
#else
#define getushort(x)	(((u_int8_t *)(x))[0] + (((u_int8_t *)(x))[1] << 8))
#define getulong(x)	(((u_int8_t *)(x))[0] + (((u_int8_t *)(x))[1] << 8) \
			 + (((u_int8_t *)(x))[2] << 16)	\
			 + (((u_int8_t *)(x))[3] << 24))
#define putushort(p, v)	(((u_int8_t *)(p))[0] = (v),	\
			 ((u_int8_t *)(p))[1] = (v) >> 8)
#define putulong(p, v)	(((u_int8_t *)(p))[0] = (v),	\
			 ((u_int8_t *)(p))[1] = (v) >> 8, \
			 ((u_int8_t *)(p))[2] = (v) >> 16,\
			 ((u_int8_t *)(p))[3] = (v) >> 24)
#endif

/*
 * BIOS Parameter Block (BPB) for DOS 3.3
 */
struct byte_bpb33 {
	int8_t bpbBytesPerSec[2];	/* bytes per sector */
	int8_t bpbSecPerClust;		/* sectors per cluster */
	int8_t bpbResSectors[2];	/* number of reserved sectors */
	int8_t bpbFATs;			/* number of FATs */
	int8_t bpbRootDirEnts[2];	/* number of root directory entries */
	int8_t bpbSectors[2];		/* total number of sectors */
	int8_t bpbMedia;		/* media descriptor */
	int8_t bpbFATsecs[2];		/* number of sectors per FAT */
	int8_t bpbSecPerTrack[2];	/* sectors per track */
	int8_t bpbHeads[2];		/* number of heads */
	int8_t bpbHiddenSecs[2];	/* number of hidden sectors */
};

/*
 * BPB for DOS 5.0 The difference is bpbHiddenSecs is a short for DOS 3.3,
 * and bpbHugeSectors is not in the 3.3 bpb.
 */
struct byte_bpb50 {
	int8_t bpbBytesPerSec[2];	/* bytes per sector */
	int8_t bpbSecPerClust;		/* sectors per cluster */
	int8_t bpbResSectors[2];	/* number of reserved sectors */
	int8_t bpbFATs;			/* number of FATs */
	int8_t bpbRootDirEnts[2];	/* number of root directory entries */
	int8_t bpbSectors[2];		/* total number of sectors */
	int8_t bpbMedia;		/* media descriptor */
	int8_t bpbFATsecs[2];		/* number of sectors per FAT */
	int8_t bpbSecPerTrack[2];	/* sectors per track */
	int8_t bpbHeads[2];		/* number of heads */
	int8_t bpbHiddenSecs[4];	/* number of hidden sectors */
	int8_t bpbHugeSectors[4];	/* # of sectors if bpbSectors == 0 */
};

/*
 * BPB for DOS 7.10 (FAT32).  This one has a few extensions to bpb50.
 */
struct byte_bpb710 {
	uint8_t bpbBytesPerSec[2];	/* bytes per sector */
	uint8_t bpbSecPerClust;		/* sectors per cluster */
	uint8_t bpbResSectors[2];	/* number of reserved sectors */
	uint8_t bpbFATs;		/* number of FATs */
	uint8_t bpbRootDirEnts[2];	/* number of root directory entries */
	uint8_t bpbSectors[2];		/* total number of sectors */
	uint8_t bpbMedia;		/* media descriptor */
	uint8_t bpbFATsecs[2];		/* number of sectors per FAT */
	uint8_t bpbSecPerTrack[2];	/* sectors per track */
	uint8_t bpbHeads[2];		/* number of heads */
	uint8_t bpbHiddenSecs[4];	/* # of hidden sectors */
	uint8_t bpbHugeSectors[4];	/* # of sectors if bpbSectors == 0 */
	uint8_t bpbBigFATsecs[4];	/* like bpbFATsecs for FAT32 */
	uint8_t bpbExtFlags[2];		/* extended flags: */
	uint8_t bpbFSVers[2];		/* filesystem version */
	uint8_t bpbRootClust[4];	/* start cluster for root directory */
	uint8_t bpbFSInfo[2];		/* filesystem info structure sector */
	uint8_t bpbBackup[2];		/* backup boot sector */
	uint8_t bpbReserved[12];	/* Reserved for future expansion */
};

/*
 * FAT32 FSInfo block.
 */
struct fsinfo {
	uint8_t fsisig1[4];
	uint8_t fsifill1[480];
	uint8_t fsisig2[4];
	uint8_t fsinfree[4];
	uint8_t fsinxtfree[4];
	uint8_t fsifill2[12];
	uint8_t fsisig3[4];
	uint8_t fsifill3[508];
	uint8_t fsisig4[4];
};
#endif /* _MSDOSFS_BPB_H_ */
