//
// dpme.h - Disk Partition Map Entry (dpme)
//
// Written by Eryk Vershen
//
// This file describes structures and values related to the standard
// Apple SCSI disk partitioning scheme.
//
// Each entry is (and shall remain) 512 bytes long.
//
// For more information see:
//	"Inside Macintosh: Devices" pages 3-12 to 3-15.
//	"Inside Macintosh - Volume V" pages V-576 to V-582
//	"Inside Macintosh - Volume IV" page IV-292
//
// There is a kernel file with much of the same info (under different names):
//	/usr/src/mklinux-1.0DR2/osfmk/src/mach_kernel/ppc/POWERMAC/mac_label.h
//

/*
 * Copyright 1996 by Apple Computer, Inc.
 *              All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation.
 *
 * APPLE COMPUTER DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL APPLE COMPUTER BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifndef __dpme__
#define __dpme__

#include "bitfield.h"

//
// Defines
//
#define	BLOCK0_SIGNATURE	0x4552	/* i.e. 'ER' */

#define	DPISTRLEN	32
#define	DPME_SIGNATURE	0x504D		/* i.e. 'PM' */

// A/UX only stuff (tradition!)
#define	dpme_bzb	dpme_boot_args
#define	BZBMAGIC 0xABADBABE	/* BZB magic number */
#define	FST	((u8) 0x1)	/* standard UNIX FS */
#define	FSTEFS	((u8) 0x2)	/* Autorecovery FS */
#define	FSTSFS	((u8) 0x3)	/* Swap FS */


//
// Types
//
typedef	unsigned char	u8;
typedef	unsigned short	u16;
typedef	unsigned long	u32;


// Physical block zero of the disk has this format
struct Block0 {
    u16 	sbSig;		/* unique value for SCSI block 0 */
    u16 	sbBlkSize;	/* block size of device */
    u32 	sbBlkCount;	/* number of blocks on device */
    u16 	sbDevType;	/* device type */
    u16 	sbDevId;	/* device id */
    u32 	sbData;		/* not used */
    u16 	sbDrvrCount;	/* driver descriptor count */
    u16 	sbMap[247];	/* descriptor map */
};
typedef struct Block0 Block0;

// Where &sbMap[0] is actually an array DDMap[sbDrvrCount]
// kludge to get around alignment junk
struct DDMap {
    u32 	ddBlock;	/* 1st driver's starting block (in sbBlkSize blocks!) */
    u16 	ddSize;		/* size of 1st driver (512-byte blks) */
    u16 	ddType;		/* system type (1 for Mac+) */
};
typedef struct DDMap DDMap;


// Each partition map entry (blocks 1 through n) has this format
struct dpme {
    u16     dpme_signature          ;
    u16     dpme_reserved_1         ;
    u32     dpme_map_entries        ;
    u32     dpme_pblock_start       ;
    u32     dpme_pblocks            ;
    char    dpme_name[DPISTRLEN]    ;  /* name of partition */
    char    dpme_type[DPISTRLEN]    ;  /* type of partition */
    u32     dpme_lblock_start       ;
    u32     dpme_lblocks            ;
    u32     dpme_flags;
#if 0
    u32     dpme_reserved_2    : 23 ;  /* Bit 9 through 31.        */
    u32     dpme_os_specific_1 :  1 ;  /* Bit 8.                   */
    u32     dpme_os_specific_2 :  1 ;  /* Bit 7.                   */
    u32     dpme_os_pic_code   :  1 ;  /* Bit 6.                   */
    u32     dpme_writable      :  1 ;  /* Bit 5.                   */
    u32     dpme_readable      :  1 ;  /* Bit 4.                   */
    u32     dpme_bootable      :  1 ;  /* Bit 3.                   */
    u32     dpme_in_use        :  1 ;  /* Bit 2.                   */
    u32     dpme_allocated     :  1 ;  /* Bit 1.                   */
    u32     dpme_valid         :  1 ;  /* Bit 0.                   */
#endif
    u32     dpme_boot_block         ;
    u32     dpme_boot_bytes         ;
    u8     *dpme_load_addr          ;
    u8     *dpme_load_addr_2        ;
    u8     *dpme_goto_addr          ;
    u8     *dpme_goto_addr_2        ;
    u32     dpme_checksum           ;
    char    dpme_process_id[16]     ;
    u32     dpme_boot_args[32]      ;
    u32     dpme_reserved_3[62]     ;
};
typedef struct dpme DPME;

#define	dpme_diskdriver_set(p, v)	bitfield_set(&p->dpme_flags, 9, 1, v)
#define	dpme_chainable_set(p, v)	bitfield_set(&p->dpme_flags, 8, 1, v)

#define	dpme_os_specific_1_set(p, v)	bitfield_set(&p->dpme_flags, 8, 1, v)
#define	dpme_os_specific_2_set(p, v)	bitfield_set(&p->dpme_flags, 7, 1, v)
#define	dpme_os_pic_code_set(p, v)	bitfield_set(&p->dpme_flags, 6, 1, v)
#define	dpme_writable_set(p, v)		bitfield_set(&p->dpme_flags, 5, 1, v)
#define	dpme_readable_set(p, v)		bitfield_set(&p->dpme_flags, 4, 1, v)
#define	dpme_bootable_set(p, v)		bitfield_set(&p->dpme_flags, 3, 1, v)
#define	dpme_in_use_set(p, v)		bitfield_set(&p->dpme_flags, 2, 1, v)
#define	dpme_allocated_set(p, v)	bitfield_set(&p->dpme_flags, 1, 1, v)
#define	dpme_valid_set(p, v)		bitfield_set(&p->dpme_flags, 0, 1, v)

#define	dpme_diskdriver_get(p)		bitfield_get(p->dpme_flags, 9, 1)
#define	dpme_chainable_get(p)		bitfield_get(p->dpme_flags, 8, 1)

#define	dpme_os_specific_1_get(p)	bitfield_get(p->dpme_flags, 8, 1)
#define	dpme_os_specific_2_get(p)	bitfield_get(p->dpme_flags, 7, 1)
#define	dpme_os_pic_code_get(p)		bitfield_get(p->dpme_flags, 6, 1)
#define	dpme_writable_get(p)		bitfield_get(p->dpme_flags, 5, 1)
#define	dpme_readable_get(p)		bitfield_get(p->dpme_flags, 4, 1)
#define	dpme_bootable_get(p)		bitfield_get(p->dpme_flags, 3, 1)
#define	dpme_in_use_get(p)		bitfield_get(p->dpme_flags, 2, 1)
#define	dpme_allocated_get(p)		bitfield_get(p->dpme_flags, 1, 1)
#define	dpme_valid_get(p)		bitfield_get(p->dpme_flags, 0, 1)


// A/UX only data structures (sentimental reasons?)

// Alternate block map (aka bad block remaping) [Never really used]
struct abm		/* altblk map info stored in bzb */
{
    u32  abm_size;	/* size of map in bytes */
    u32  abm_ents;	/* number of used entries */
    u32  abm_start;	/* start of altblk map */
};
typedef	struct abm ABM;

// BZB (Block Zero Block, but I can't remember the etymology)
// Where &dpme_boot_args[0] is actually the address of a struct bzb
// kludge to get around alignment junk
struct	bzb			/* block zero block format */
{
    u32  bzb_magic;		/* magic number */
    u8   bzb_cluster;		/* Autorecovery cluster grouping */
    u8   bzb_type;		/* FS type */
    u16  bzb_inode;		/* bad block inode number */
    u32  bzb_flags;
#if 0
    u16  bzb_root:1,		/* FS is a root FS */
	 bzb_usr:1,		/* FS is a usr FS */
	 bzb_crit:1,		/* FS is a critical FS */
	 bzb_rsrvd:8,		/* reserved for later use */
	 bzb_slice:5;		/* slice number to associate with plus one */
    u16  bzb_filler;		/* pad bitfield to 32 bits */
#endif
    u32  bzb_tmade;		/* time of FS creation */
    u32  bzb_tmount;		/* time of last mount */
    u32  bzb_tumount;		/* time of last umount */
    ABM  bzb_abm;		/* altblk map info */
    u32  bzb_fill2[7];		/* for expansion of ABM (ha!ha!) */
    u8   bzb_mount_point[64];	/* default mount point name */
};
typedef	struct bzb	BZB;

#define	bzb_root_set(p, v)		bitfield_set(&p->bzb_flags, 31, 1, v)
#define	bzb_usr_set(p, v)		bitfield_set(&p->bzb_flags, 30, 1, v)
#define	bzb_crit_set(p, v)		bitfield_set(&p->bzb_flags, 29, 1, v)
#define	bzb_slice_set(p, v)		bitfield_set(&p->bzb_flags, 20, 5, v)

#define	bzb_root_get(p)			bitfield_get(p->bzb_flags, 31, 1)
#define	bzb_usr_get(p)			bitfield_get(p->bzb_flags, 30, 1)
#define	bzb_crit_get(p)			bitfield_get(p->bzb_flags, 29, 1)
#define	bzb_slice_get(p)		bitfield_get(p->bzb_flags, 20, 5)


//
// Global Constants
//


//
// Global Variables
//


//
// Forward declarations
//

#endif /* __dpme__ */
