/*	$NetBSD: bootsect.h,v 1.7 1995/07/24 06:36:23 leo Exp $	*/

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

/*
 * Format of a boot sector.  This is the first sector on a DOS floppy disk
 * or the fist sector of a partition on a hard disk.  But, it is not the
 * first sector of a partitioned hard disk.
 */
struct bootsector33 {
	u_int8_t	bsJump[3];		/* jump inst E9xxxx or EBxx90 */
	int8_t		bsOemName[8];		/* OEM name and version */
	int8_t		bsBPB[19];		/* BIOS parameter block */
	int8_t		bsDriveNumber;		/* drive number (0x80) */
	int8_t		bsBootCode[479];	/* pad so struct is 512b */
	u_int16_t	bsBootSectSig;
#define	BOOTSIG	0xaa55
};

struct bootsector50 {
	u_int8_t	bsJump[3];		/* jump inst E9xxxx or EBxx90 */
	int8_t		bsOemName[8];		/* OEM name and version */
	int8_t		bsBPB[25];		/* BIOS parameter block */
	int8_t		bsDriveNumber;		/* drive number (0x80) */
	int8_t		bsReserved1;		/* reserved */
	int8_t		bsBootSignature;	/* ext. boot signature (0x29) */
#define	EXBOOTSIG	0x29
	int8_t		bsVolumeID[4];		/* volume ID number */
	int8_t		bsVolumeLabel[11];	/* volume label */
	int8_t		bsFileSysType[8];	/* fs type (FAT12 or FAT16) */
	int8_t		bsBootCode[448];	/* pad so structure is 512b */
	u_int16_t	bsBootSectSig;
#define	BOOTSIG	0xaa55
};
#ifdef	atari
/*
 * The boot sector on a gemdos fs is a little bit different from the msdos fs
 * format. Currently there is no need to declare a seperate structure, the
 * bootsector33 struct will do.
 */
#if 0
struct bootsec_atari {
	u_int8_t	bsBranch[2];		/* branch inst if auto-boot	*/
	int8_t		bsFiller[6];		/* anything or nothing		*/
	int8_t		bsSerial[3];		/* serial no. for mediachange	*/
	int8_t		bsBPB[19];		/* BIOS parameter block		*/
	int8_t		bsBootCode[482];	/* pad so struct is 512b	*/
};
#endif
#endif /* atari */

union bootsector {
	struct bootsector33 bs33;
	struct bootsector50 bs50;
};

/*
 * Shorthand for fields in the bpb.
 */
#define	bsBytesPerSec	bsBPB.bpbBytesPerSec
#define	bsSectPerClust	bsBPB.bpbSectPerClust
#define	bsResSectors	bsBPB.bpbResSectors
#define	bsFATS		bsBPB.bpbFATS
#define	bsRootDirEnts	bsBPB.bpbRootDirEnts
#define	bsSectors	bsBPB.bpbSectors
#define	bsMedia		bsBPB.bpbMedia
#define	bsFATsecs	bsBPB.bpbFATsecs
#define	bsSectPerTrack	bsBPB.bpbSectPerTrack
#define	bsHeads		bsBPB.bpbHeads
#define	bsHiddenSecs	bsBPB.bpbHiddenSecs
#define	bsHugeSectors	bsBPB.bpbHugeSectors
