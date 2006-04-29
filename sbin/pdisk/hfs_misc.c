//
// hfs_misc.c - hfs routines
//
// Written by Eryk Vershen
//

/*
 * Copyright 2000 by Eryk Vershen
 */

// for *printf()
#include <stdio.h>

// for malloc(), calloc() & free()
#include <stdlib.h>

// for strncpy() & strcmp()
#include <string.h>
// for O_RDONLY & O_RDWR
#include <fcntl.h>
// for errno
#include <errno.h>

#include "hfs_misc.h"
#include "partition_map.h"
#include "convert.h"
#include "errors.h"


//
// Defines
//
#define MDB_OFFSET	2
#define HFS_SIG		0x4244	/* i.e 'BD' */
#define HFS_PLUS_SIG	0x482B	/* i.e 'H+' */

#define get_align_long(x)	(*(u32*)(x))


//
// Types
//
typedef long long u64;

typedef struct ExtDescriptor {		// extent descriptor
    u16	xdrStABN;	// first allocation block
    u16	xdrNumABlks;	// number of allocation blocks
} ext_descriptor;

typedef struct ExtDataRec {
    ext_descriptor	ed[3];	// extent data record
} ext_data_rec;

/*
 * The crazy "u16 x[2]" stuff here is to get around the fact
 * that I can't convince the Mac compiler to align on 32 bit
 * quantities on 16 bit boundaries...
 */
struct mdb_record {		// master directory block
    u16	drSigWord;	// volume signature
    u16	drCrDate[2];	// date and time of volume creation
    u16	drLsMod[2];	// date and time of last modification
    u16	drAtrb;		// volume attributes
    u16	drNmFls;	// number of files in root directory
    u16	drVBMSt;	// first block of volume bitmap
    u16	drAllocPtr;	// start of next allocation search
    u16	drNmAlBlks;	// number of allocation blocks in volume
    u32	drAlBlkSiz;	// size (in bytes) of allocation blocks
    u32	drClpSiz;	// default clump size
    u16	drAlBlSt;	// first allocation block in volume
    u16	drNxtCNID[2];	// next unused catalog node ID
    u16	drFreeBks;	// number of unused allocation blocks
    char	drVN[28];	// volume name
    u16	drVolBkUp[2];	// date and time of last backup
    u16	drVSeqNum;	// volume backup sequence number
    u16	drWrCnt[2];	// volume write count
    u16	drXTClpSiz[2];	// clump size for extents overflow file
    u16	drCTClpSiz[2];	// clump size for catalog file
    u16	drNmRtDirs;	// number of directories in root directory
    u32	drFilCnt;	// number of files in volume
    u32	drDirCnt;	// number of directories in volume
    u32	drFndrInfo[8];	// information used by the Finder
#ifdef notdef
    u16	drVCSize;	// size (in blocks) of volume cache
    u16	drVBMCSize;	// size (in blocks) of volume bitmap cache
    u16	drCtlCSize;	// size (in blocks) of common volume cache
#else
    u16	drEmbedSigWord;	// type of embedded volume
    ext_descriptor	drEmbedExtent;	// embedded volume extent
#endif
    u16	drXTFlSize[2];	// size of extents overflow file
    ext_data_rec	drXTExtRec;	// extent record for extents overflow file
    u16	drCTFlSize[2];	// size of catalog file
    ext_data_rec	drCTExtRec;	// extent record for catalog file
};


typedef u32 HFSCatalogNodeID;

typedef struct HFSPlusExtentDescriptor {
    u32 startBlock;
    u32 blockCount;
} HFSPlusExtentDescriptor;

typedef HFSPlusExtentDescriptor HFSPlusExtentRecord[ 8];

typedef struct HFSPlusForkData {
    u64 logicalSize;
    u32 clumpSize;
    u32 totalBlocks;
    HFSPlusExtentRecord extents;
} HFSPlusForkData;

struct HFSPlusVolumeHeader {
    u16 signature;
    u16 version;
    u32 attributes;
    u32 lastMountedVersion;
    u32 reserved;
    u32 createDate;
    u32 modifyDate;
    u32 backupDate;
    u32 checkedDate;
    u32 fileCount;
    u32 folderCount;
    u32 blockSize;
    u32 totalBlocks;
    u32 freeBlocks;
    u32 nextAllocation;
    u32 rsrcClumpSize;
    u32 dataClumpSize;
    HFSCatalogNodeID nextCatalogID;
    u32 writeCount;
    u64 encodingsBitmap;
    u8 finderInfo[ 32];
    HFSPlusForkData allocationFile;
    HFSPlusForkData extentsFile;
    HFSPlusForkData catalogFile;
    HFSPlusForkData attributesFile;
    HFSPlusForkData startupFile;
} HFSPlusVolumeHeader;


//
// Global Constants
//


//
// Global Variables
//


//
// Forward declarations
//
u32 embeded_offset(struct mdb_record *mdb, u32 sector);
int read_partition_block(partition_map *entry, unsigned long num, char *buf);


//
// Routines
//
u32
embeded_offset(struct mdb_record *mdb, u32 sector)
{
    u32 e_offset;

    e_offset = mdb->drAlBlSt + mdb->drEmbedExtent.xdrStABN * (mdb->drAlBlkSiz / 512);

    return e_offset + sector;
}


char *
get_HFS_name(partition_map *entry, int *kind)
{
    DPME *data;
    struct mdb_record *mdb;
    //struct HFSPlusVolumeHeader *mdb2;
    char *name = NULL;
    int len;

    *kind = kHFS_not;

    mdb = (struct mdb_record *) malloc(PBLOCK_SIZE);
    if (mdb == NULL) {
	error(errno, "can't allocate memory for MDB");
	return NULL;
    }

    data = entry->data;
    if (strcmp(data->dpme_type, kHFSType) == 0) {
	if (read_partition_block(entry, 2, (char *)mdb) == 0) {
	    error(-1, "Can't read block %d from partition %d", 2, entry->disk_address);
	    goto not_hfs;
	}
	if (mdb->drSigWord == HFS_PLUS_SIG) {
	    // pure HFS Plus
	    // printf("%lu HFS Plus\n", entry->disk_address);
	    *kind = kHFS_plus;
	} else if (mdb->drSigWord != HFS_SIG) {
	    // not HFS !!!
	    printf("%lu not HFS\n", entry->disk_address);
	    *kind = kHFS_not;
	} else if (mdb->drEmbedSigWord != HFS_PLUS_SIG) {
	    // HFS
	    // printf("%lu HFS\n", entry->disk_address);
	    *kind = kHFS_std;
	    len = mdb->drVN[0];
	    name = (char *) malloc(len+1);
	    strncpy(name, &mdb->drVN[1], len);
	    name[len] = 0;
	} else {
	    // embedded HFS plus
	    // printf("%lu embedded HFS Plus\n", entry->disk_address);
	    *kind = kHFS_embed;
	    len = mdb->drVN[0];
	    name = (char *) malloc(len+1);
	    strncpy(name, &mdb->drVN[1], len);
	    name[len] = 0;
	}
    }
not_hfs:
    free(mdb);
    return name;
}

// really need a function to read block n from partition m

int
read_partition_block(partition_map *entry, unsigned long num, char *buf)
{
    DPME *data;
    partition_map_header * map;
    u32 base;
    u64 offset;

    map = entry->the_map;
    data = entry->data;
    base = data->dpme_pblock_start;

    if (num >= data->dpme_pblocks) {
	return 0;
    }
    offset = ((long long) base) * map->logical_block + num * 512;

    return read_media(map->m, offset, 512, (void *)buf);
}
