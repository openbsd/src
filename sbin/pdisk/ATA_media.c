/*
 * ATA_media.c -
 *
 * Written by Eryk Vershen (eryk@apple.com)
 */

/*
 * Copyright 1997,1998 by Apple Computer, Inc.
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


// for printf()
#include <stdio.h>
// for malloc() & free()
#include <stdlib.h>
#include <ATA.h>
// for SCSI command structures
#include "MacSCSICommand.h"
#include "ATA_media.h"
#include "util.h"


/*
 * Defines
 */
#define RESULT_OFFSET(type) \
    ((sizeof(type) == 1) ? 3 : ((sizeof(type) == 2) ? 1 : 0))
#define TBTrapTableAddress(trapNum) (((trapNum & 0x03FF) << 2) + 0xE00)
#define SWAP_SHORTS(x)  ((((x) & 0xFFFF) << 16) | (((x) >> 16) & 0xFFFF))
#define LBA_CAPABLE 0x0200


/*
 * Types
 */
typedef struct ATA_info *ATA_INFO;

struct ATA_info {
    long	    lba;
    long	    heads;
    long	    sectors;
};

typedef struct ATA_media *ATA_MEDIA;

struct ATA_media {
    struct media    m;
    long            id;
    struct ATA_info info;
};

struct ATA_manager {
    long        exists;
    long        kind;
    struct {
	char    major;
	char    minor;
    } version;
    short       busCount;
};

typedef struct ATA_media_iterator *ATA_MEDIA_ITERATOR;

struct ATA_media_iterator {
    struct media_iterator   m;
    long                    bus;
    long                    id;
};

struct ATA_identify_drive_info {        /* word */
    unsigned short  config_bits;        /*  0 */
    unsigned short  num_cylinders;      /*  1 */
    unsigned short  reserved2;          /*  2 */
    unsigned short  num_heads;          /*  3 */
    unsigned short  bytes_per_track;    /*  4 */
    unsigned short  bytes_per_sector;   /*  5 */
    unsigned short  sectors_per_track;  /*  6 */
    unsigned short  vendor7[3];         /*  7-9 */
    char            serial_number[20];  /* 10-19 */
    unsigned short  buffer_type;        /* 20 */
    unsigned short  buffer_size;        /* 21 */
    unsigned short  num_of_ecc_bytes;   /* 22 */
    char            firmware_rev[8];    /* 23-26 */
    char            model_number[40];   /* 27-46 */
    unsigned short  word47;             /* 47 */
    unsigned short  double_word_io;     /* 48 */
    unsigned short  capabilities;       /* 49 */
    unsigned short  reserved50;         /* 50 */
    unsigned short  pio_timing;         /* 51 */
    unsigned short  dma_timing;         /* 52 */
    unsigned short  current_is_valid;   /* 53 */
    unsigned short  cur_cylinders;      /* 54 */
    unsigned short  cur_heads;          /* 55 */
    unsigned short  cur_sec_per_track;  /* 56 */
    unsigned long   total_sectors;      /* 57-58 */
    unsigned short  multiple_sectors;   /* 59 */
    unsigned long   lba_sectors;        /* 60-61 */
    unsigned short  singleword_dma;     /* 62 */
    unsigned short  multiword_dma;      /* 63 */
    unsigned short  reserved64[64];     /* 64-127 */
    unsigned short  vendor128[32];      /* 128-159 */
    unsigned short  reserved160[96];    /* 160-255 */
};

struct ATAPI_identify_drive_info {      /* word */
    unsigned short  config_bits;        /*  0 */
    unsigned short  retired1[9];        /*  1-9 */
    char            serial_number[20];  /* 10-19 */
    unsigned short  retired20[3];       /* 20-22 */
    char            firmware_rev[8];    /* 23-26 */
    char            model_number[40];   /* 27-46 */
    unsigned short  retired47[2];       /* 47-48 */
    unsigned short  capabilities;       /* 49 */
    unsigned short  reserved50;         /* 50 */
    unsigned short  pio_timing;         /* 51 */
    unsigned short  dma_timing;         /* 52 */
    unsigned short  current_is_valid;   /* 53 */
    unsigned short  retired54[8];       /* 54-61 */
    unsigned short  singleword_dma;     /* 62 */
    unsigned short  multiword_dma;      /* 63 */
    unsigned short  pio_transfer;       /* 64 */
    unsigned short  min_cycle_time;     /* 65 */
    unsigned short  rec_cycle_time;     /* 66 */
    unsigned short  min_wo_flow;        /* 67 */
    unsigned short  min_with_flow;      /* 68 */
    unsigned short  reserved69[2];      /* 69-70 */
    unsigned short  release_over;       /* 71 */
    unsigned short  release_service;    /* 72 */
    unsigned short  major_rev;          /* 73 */
    unsigned short  minor_rev;          /* 74 */
    unsigned short  reserved75[53];     /* 75-127 */
    unsigned short  vendor128[32];      /* 128-159 */
    unsigned short  reserved160[96];    /* 160-255 */
};

/* Identifies the bus protocol type. */
enum {
    kDevUnknown     =   0,
    kDevATA         =   1,
    kDevATAPI       =   2,
    kDevPCMCIA      =   3
};


/*
 * Global Constants
 */
enum {
    kNoDevice = 0x00FF,
    kATAtimeout = 3000,
    kATAcmdATAPIPacket          = 0x00A0                        /* ATAPI packet command */
};


/*
 * Global Variables
 */
static long ata_inited = 0;
static struct ATA_manager ata_mgr;

/*
 * Forward declarations
 */
int ATAManagerPresent(void);
int ATAHardwarePresent(void);
pascal SInt16 ataManager(ataPB *pb);
void ata_init(void);
ATA_MEDIA new_ata_media(void);
long read_ata_media(MEDIA m, long long offset, unsigned long count, void *address);
long write_ata_media(MEDIA m, long long offset, unsigned long count, void *address);
long close_ata_media(MEDIA m);
long os_reload_ata_media(MEDIA m);
long compute_id(long bus, long device);
pascal SInt16 ataManager(ataPB *pb);
int ATA_ReadBlock(UInt32 deviceID, ATA_INFO info, UInt32 block_size, UInt32 block, UInt8 *address);
int ATA_WriteBlock(UInt32 deviceID, ATA_INFO info, UInt32 block_size, UInt32 block, UInt8 *address);
long get_info(long id, struct ATA_identify_drive_info *ip);
long get_pi_info(long id, struct ATAPI_identify_drive_info *ip);
long is_atapi(long id);
long read_atapi_media(MEDIA m, long long offset, unsigned long count, void *address);
long write_atapi_media(MEDIA m, long long offset, unsigned long count, void *address);
int ATAPI_ReadBlock(UInt32 deviceID, UInt32 block_size, UInt32 block, UInt8 *address);
int ATAPI_TestUnitReady(UInt32 deviceID);
int ATAPI_ReadCapacity(UInt32 deviceID, unsigned long *block_size, unsigned long *blocks);
ATA_MEDIA_ITERATOR new_ata_iterator(void);
void reset_ata_iterator(MEDIA_ITERATOR m);
char *step_ata_iterator(MEDIA_ITERATOR m);
void delete_ata_iterator(MEDIA_ITERATOR m);


/*
 * Routines
 */
#if GENERATINGPOWERPC
pascal SInt16
ataManager(ataPB *pb)
{
    #ifdef applec
	#if sizeof(SInt16) > 4
	    #error "Result types larger than 4 bytes are not supported."
	#endif
    #endif
    long    private_result;

    private_result = CallUniversalProc(
	*(UniversalProcPtr*)TBTrapTableAddress(0xAAF1),
	kPascalStackBased
	 | RESULT_SIZE(SIZE_CODE(sizeof(SInt16)))
	 | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(pb))),
	pb);
    return *(((SInt16*)&private_result) + RESULT_OFFSET(SInt16));
}
#endif


int
ATAHardwarePresent(void)
{
    UInt16  configFlags;

    // Hardware configuration flags
    configFlags = LMGetHWCfgFlags();

    return ((configFlags & 0x0080) != 0);
}


int
ATAManagerPresent(void)
{
    if (ATAHardwarePresent()) {
	return (TrapAvailable(kATATrap));
    } else {
	return 0;
    }
}

void
ata_init(void)
{
    ataMgrInquiry   pb;
    OSErr           status;

    if (ata_inited != 0) {
	return;
    }
    ata_inited = 1;

    if (ATAManagerPresent() == 0) {
	ata_mgr.exists = 0;
	return;
    }

    ata_mgr.exists = 1;
    ata_mgr.kind = allocate_media_kind();

    clear_memory((void *)&pb, sizeof(pb));

    pb.ataPBFunctionCode =  kATAMgrManagerInquiry;
    pb.ataPBVers =          kATAPBVers1;

    status = ataManager((ataPB*) &pb );

    if (status != noErr) {
	ata_mgr.exists = 0;
	return;
    }
    ata_mgr.version.major = pb.ataMgrVersion.majorRev;
    ata_mgr.version.minor = pb.ataMgrVersion.minorAndBugRev >> 4;
    ata_mgr.busCount = pb.ataBusCnt;
}


ATA_MEDIA
new_ata_media(void)
{
    return (ATA_MEDIA) new_media(sizeof(struct ATA_media));
}


#pragma mark -


long
compute_id(long bus, long device)
{
    long id;

    id = -1;
    if (ata_mgr.version.major < 3) {
	if (device != 0) {
	    /* bad device id */
	} else if (bus >= ata_mgr.busCount) {
	    /* bad bus id */
	} else {
	    id = bus & 0xFF;
	}
    } else {
	if (device < 0 || device > 1) {
	    /* bad device id */
	} else if (bus >= ata_mgr.busCount) {
	    /* bad bus id */
	} else {
	    id = ((device & 0xFF) << 8) | (bus & 0xFF);
	}
    }
    return id;
}


static long
get_info(long id, struct ATA_identify_drive_info *ip)
{
    ataIdentify     pb;
    ataDevConfiguration pb2;
    OSErr           status;
    long            rtn_value;
    long            atapi;

    if (sizeof(struct ATA_identify_drive_info) < 512) {
	return 0;
    }
    clear_memory((void *)ip, sizeof(struct ATA_identify_drive_info));

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrDriveIdentify;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   id;
    pb.ataPBFlags           =   mATAFlagIORead | mATAFlagByteSwap;
    pb.ataPBTimeOut         =   kATAtimeout;
    pb.ataPBBuffer          =   (void*) ip;

    status = ataManager((ataPB*) &pb );

    if (status != noErr) {
	//printf("get info status = %d\n", status);
	rtn_value = 0;
    } else {
	ip->total_sectors = SWAP_SHORTS(ip->total_sectors);
	ip->lba_sectors = SWAP_SHORTS(ip->lba_sectors);
	rtn_value = 1;
    }
    return rtn_value;
}


static long
is_atapi(long id)
{
    ataDevConfiguration pb;
    OSErr           status;
    long            atapi;

    atapi = 0;
    if (ata_mgr.version.major >= 2) {
	clear_memory((void *)&pb, sizeof(pb));
	pb.ataPBFunctionCode    =   kATAMgrGetDrvConfiguration;
	pb.ataPBVers            =   kATAPBVers2;
	pb.ataPBDeviceID        =   id;
	pb.ataPBTimeOut     =   kATAtimeout;
	
	status = ataManager((ataPB*) &pb );
	if (status != noErr) {
	    //printf("is atatpi status = %d\n", status);
	} else if (pb.ataDeviceType == kDevATAPI) {
	    atapi = 1;
	    /* the drive can be asleep or something in which case this doesn't work */
	    /* how do we do reads */
	}
    }
    return atapi;
}


MEDIA
open_ata_as_media(long bus, long device)
{
    ATA_MEDIA   a;
    long        id;
    struct ATA_identify_drive_info  info;
    unsigned char *buf;
    unsigned long total;

    if (ata_inited == 0) {
	ata_init();
    }

    if (ata_mgr.exists == 0) {
	//printf("ATA manager does not exist\n");
	return 0;
    }

    id = compute_id(bus, device);

    if (id < 0) {
    	return 0;

    } else if (is_atapi(id)) {
	a = (ATA_MEDIA) open_atapi_as_media(bus, device);

    } else {
	a = 0;
	if (get_info(id, &info) != 0) {
	    a = new_ata_media();
	    if (a != 0) {
		a->m.kind = ata_mgr.kind;
		if ((info.capabilities & LBA_CAPABLE) != 0) {
		    total = info.lba_sectors;
		    a->info.lba = 1;
		    a->info.heads = 0;
		    a->info.sectors = 0;
		} else {
		    /* Only CHS - Cylinder Head Sector addressing */
		    total = info.total_sectors;
		    a->info.lba = 0;
		    a->info.heads = info.cur_heads;
		    a->info.sectors = info.cur_sec_per_track;
		}
		if (info.bytes_per_sector == 0) {
		    buf = malloc(2048);
		    if (ATA_ReadBlock(id, &a->info, 512, 0, buf)) {
			a->m.grain = 512; /* XXX not right */
		    } else if (ATA_ReadBlock(id, &a->info, 2048, 0, buf)) {
			a->m.grain = 2048; /* XXX not right */
		    }
		    free(buf);
		} else {
		    a->m.grain = info.bytes_per_sector;
		}
		if (total == 0) {
		    a->m.size_in_bytes = ((long long)1000) * a->m.grain; /* XXX not right */
		} else {
		    a->m.size_in_bytes = ((long long)total) * a->m.grain;
		}
		a->m.do_read = read_ata_media;
		a->m.do_write = write_ata_media;
		a->m.do_close = close_ata_media;
		a->m.do_os_reload = os_reload_ata_media;
		a->id = id;
	    }
	} else {
	    printf("ATA - couldn't get info\n");
	}
    }
    return (MEDIA) a;
}


long
read_ata_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    ATA_MEDIA a;
    ataIOPB pb;
    OSErr       status;
    long rtn_value;
    long block;
    long block_count;
    long block_size;
    unsigned char *buffer;
    int i;

    a = (ATA_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != ata_mgr.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (offset + count > a->m.size_in_bytes) {
	/* check for offset (and offset+count) too large */
    } else {
	/* do a read on the physical device */
	block_size = a->m.grain;
	block = offset / block_size;
	block_count = count / block_size;
	buffer = address;
	rtn_value = 1;
	for (i = 0; i < block_count; i++) {
	    if (ATA_ReadBlock(a->id, &a->info, block_size, block, buffer) == 0) {
		rtn_value = 0;
		break;
	    }
	    buffer += block_size;
	    block += 1;
	}
    }
    return rtn_value;
}


long
write_ata_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    ATA_MEDIA a;
    long rtn_value;
    long block;
    long block_count;
    long block_size;
    unsigned char *buffer;
    int i;

    a = (ATA_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != ata_mgr.kind) {
	/* XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (offset + count > a->m.size_in_bytes) {
	/* check for offset (and offset+count) too large */
    } else {
	/* do a write on the physical device */
	block_size = a->m.grain;
	block = offset / block_size;
	block_count = count / block_size;
	buffer = address;
	rtn_value = 1;
	for (i = 0; i < block_count; i++) {
	    if (ATA_WriteBlock(a->id, &a->info, block_size, block, buffer) == 0) {
		rtn_value = 0;
		break;
	    }
	    buffer += block_size;
	    block += 1;
	}
    }
    return rtn_value;
}


long
close_ata_media(MEDIA m)
{
    ATA_MEDIA a;

    a = (ATA_MEDIA) m;
    if (a == 0) {
	return 0;
    } else if (a->m.kind != ata_mgr.kind) {
	/* XXX need to error here - this is an internal problem */
	return 0;
    }
    /* XXX nothing to do - I think? */
    return 1;
}


long
os_reload_ata_media(MEDIA m)
{
    printf("Reboot your system so the partition table will be reread.\n");
    return 1;
}


int
ATA_ReadBlock(UInt32 deviceID, ATA_INFO info, UInt32 block_size, UInt32 block, UInt8 *address)
{
    ataIOPB     pb;
    OSErr       status;
    long        slave;
    long	lba, cyl, head, sector;

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrExecIO;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   deviceID;
    pb.ataPBFlags           =   mATAFlagTFRead | mATAFlagIORead ;
    pb.ataPBTimeOut         =   kATAtimeout;

    pb.ataPBLogicalBlockSize =  block_size;
    pb.ataPBBuffer          =   address;
    pb.ataPBByteCount = block_size;
    if (info->lba) {
    	lba = 0x40;
    	sector = block & 0xFF;
    	head = (block >> 24) & 0xF;
    	cyl = (block >> 8) & 0xFFFF;
    } else {
    	lba = 0x00;
	sector = (block % info->sectors) + 1;
	cyl = block / info->sectors;
	head = cyl % info->heads;
	cyl = cyl / info->heads;
    }

    pb.ataPBTaskFile.ataTFCount = 1;
    pb.ataPBTaskFile.ataTFSector = sector;
    pb.ataPBTaskFile.ataTFCylinder = cyl;
    if (deviceID & 0x0FF00) {
	slave = 0x10;
    } else {
	slave = 0x0;
    }
			      /* std | L/C  | Drive | head */
    pb.ataPBTaskFile.ataTFSDH = 0xA0 | lba | slave | head;
    pb.ataPBTaskFile.ataTFCommand = kATAcmdRead;

    status = ataManager((ataPB*) &pb );
    if (status != noErr) {
	/* failure */
	//printf(" ATA read status = %d\n", status);
	return 0;
    } else {
	return 1;
    }
}


int
ATA_WriteBlock(UInt32 deviceID, ATA_INFO info, UInt32 block_size, UInt32 block, UInt8 *address)
{
    ataIOPB     pb;
    OSErr       status;
    long        slave;
    long	lba, cyl, head, sector;

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrExecIO;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   deviceID;
    pb.ataPBFlags           =   mATAFlagTFRead | mATAFlagIOWrite ;
    pb.ataPBTimeOut         =   kATAtimeout;

    pb.ataPBLogicalBlockSize =  block_size;
    pb.ataPBBuffer          =   address;
    pb.ataPBByteCount = block_size;
    if (info->lba) {
    	lba = 0x40;
    	sector = block & 0xFF;
    	head = (block >> 24) & 0xF;
    	cyl = (block >> 8) & 0xFFFF;
    } else {
    	lba = 0x00;
	sector = (block % info->sectors) + 1;
	cyl = block / info->sectors;
	head = cyl % info->heads;
	cyl = cyl / info->heads;
    }
    pb.ataPBTaskFile.ataTFCount = 1;
    pb.ataPBTaskFile.ataTFSector = sector;
    pb.ataPBTaskFile.ataTFCylinder = cyl;
    if (deviceID & 0x0FF00) {
	slave = 0x10;
    } else {
	slave = 0x0;
    }
			      /* std | L/C  | Drive | head */
    pb.ataPBTaskFile.ataTFSDH = 0xA0 | lba | slave | head;
    pb.ataPBTaskFile.ataTFCommand = kATAcmdWrite;

    status = ataManager((ataPB*) &pb );
    if (status != noErr) {
	/* failure */
	return 0;
    } else {
	return 1;
    }
}


#pragma mark -


/*
 * ATAPI stuff
 */
static long
get_pi_info(long id, struct ATAPI_identify_drive_info *ip)
{
    ataIdentify     pb;
    OSErr           status;
    long            rtn_value;

    if (sizeof(struct ATAPI_identify_drive_info) < 512) {
	return 0;
    }
    clear_memory((void *)ip, sizeof(struct ATAPI_identify_drive_info));

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrDriveIdentify;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   id;
    pb.ataPBFlags           =   mATAFlagIORead | mATAFlagByteSwap | mATAFlagProtocol1;
    pb.ataPBTimeOut         =   kATAtimeout;
    pb.ataPBBuffer          =   (void*) ip;

    status = ataManager((ataPB*) &pb );

    if (status != noErr) {
	//printf("get pi info status = %d\n", status);
	rtn_value = 0;
    } else {
	rtn_value = 1;
    }
    return rtn_value;
}


MEDIA
open_atapi_as_media(long bus, long device)
{
    ATA_MEDIA   a;
    long        id;
    struct ATAPI_identify_drive_info    info;
    unsigned char *buf;
    unsigned long block_size;
    unsigned long blocks;

    if (ata_inited == 0) {
	ata_init();
    }

    if (ata_mgr.exists == 0) {
	return 0;
    }

    id = compute_id(bus, device);

    if (!is_atapi(id)) {
	a = 0;

    } else {
	a = 0;
	if (get_pi_info(id, &info) != 0
		&& (info.capabilities & LBA_CAPABLE) != 0) {
	    if (ATAPI_TestUnitReady(id) != 0) {
		a = new_ata_media();
		if (a != 0) {
		    a->m.kind = ata_mgr.kind;
		    if (ATAPI_ReadCapacity(id, &block_size, &blocks) == 0) {
			block_size = 2048;
			blocks = 1000;
		    }
		    a->m.grain = block_size;
		    a->m.size_in_bytes = ((long long)blocks) * a->m.grain;
		    a->m.do_read = read_atapi_media;
		    a->m.do_write = write_atapi_media;
		    a->m.do_close = close_ata_media;
		    a->m.do_os_reload = os_reload_ata_media;
		    a->id = id;
		}
	    } else {
		printf("ATAPI - unit not ready\n");
	    }
	} else {
	    printf("ATAPI - couldn't get info or not LBA capable\n");
	}
    }
    return (MEDIA) a;
}


long
read_atapi_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    ATA_MEDIA a;
    ataIOPB pb;
    OSErr       status;
    long rtn_value;
    long block;
    long block_count;
    long block_size;
    unsigned char *buffer;
    int i;

    a = (ATA_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != ata_mgr.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (offset + count > a->m.size_in_bytes) {
	/* check for offset (and offset+count) too large */
    } else {
	/* XXX do a read on the physical device */
	block_size = a->m.grain;
	block = offset / block_size;
	block_count = count / block_size;
	buffer = address;
	rtn_value = 1;
	for (i = 0; i < block_count; i++) {
	    if (ATAPI_ReadBlock(a->id, block_size, block, buffer) == 0) {
		rtn_value = 0;
		break;
	    }
	    buffer += block_size;
	    block += 1;
	}
    }
    return rtn_value;
}


long
write_atapi_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    return 0;
}


int
ATAPI_ReadBlock(UInt32 deviceID, UInt32 block_size, UInt32 block, UInt8 *address)
{
    ataIOPB         pb;
    OSErr           status;
    long            slave;
    ATAPICmdPacket  cmdPacket;
    SCSI_10_Byte_Command *gRead;
    long count;

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrExecIO;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   deviceID;
    pb.ataPBFlags           =   mATAFlagTFRead | mATAFlagIORead | mATAFlagProtocol1;
    pb.ataPBTimeOut         =   kATAtimeout;

    pb.ataPBBuffer          =   address;
    pb.ataPBByteCount = block_size;
    pb.ataPBTaskFile.ataTFCylinder = block_size;
    if (deviceID & 0x0FF00) {
	slave = 0x10;
    } else {
	slave = 0x0;
    }
			      /* std | L/C  | Drive | head */
    pb.ataPBTaskFile.ataTFSDH = 0xA0 | 0x40 | slave;
    pb.ataPBTaskFile.ataTFCommand = kATAcmdATAPIPacket;
    pb.ataPBPacketPtr = &cmdPacket;

    cmdPacket.atapiPacketSize = 16;
    clear_memory((void *)&cmdPacket.atapiCommandByte, 16);
    gRead = (SCSI_10_Byte_Command *) &cmdPacket.atapiCommandByte[0];

    gRead->opcode = kScsiCmdRead10;

    gRead->lbn4 = (block >> 24) & 0xFF;
    gRead->lbn3 = (block >> 16) & 0xFF;
    gRead->lbn2 = (block >> 8) & 0xFF;
    gRead->lbn1 = block & 0xFF;

    count = 1;
    gRead->len2 = (count >> 8) & 0xFF;
    gRead->len1 = count & 0xFF;


    status = ataManager((ataPB*) &pb );
    if (status != noErr) {
	/* failure */
	//printf("ATAPI read status = %d\n", status);
	return 0;
    } else {
	return 1;
    }
}


int
ATAPI_TestUnitReady(UInt32 deviceID)
{
    ataIOPB         pb;
    OSErr           status;
    long            slave;
    ATAPICmdPacket  cmdPacket;
    SCSI_10_Byte_Command *gTestUnit;

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrExecIO;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   deviceID;
    pb.ataPBFlags           =   mATAFlagTFRead | mATAFlagIORead | mATAFlagProtocol1;
    pb.ataPBTimeOut         =   kATAtimeout;

    if (deviceID & 0x0FF00) {
	slave = 0x10;
    } else {
	slave = 0x0;
    }
			      /* std | L/C  | Drive | head */
    pb.ataPBTaskFile.ataTFSDH = 0xA0 | 0x40 | slave;
    pb.ataPBTaskFile.ataTFCommand = kATAcmdATAPIPacket;
    pb.ataPBPacketPtr = &cmdPacket;

    cmdPacket.atapiPacketSize = 16;
    clear_memory((void *)&cmdPacket.atapiCommandByte, 16);
    gTestUnit = (SCSI_10_Byte_Command *) &cmdPacket.atapiCommandByte[0];

    gTestUnit->opcode = kScsiCmdTestUnitReady;


    status = ataManager((ataPB*) &pb );
    if (status != noErr) {
	/* failure */
	//printf("ATAPI test unit ready status = %d\n", status);
	return 0;
    } else {
	return 1;
    }
}


int
ATAPI_ReadCapacity(UInt32 deviceID, unsigned long *block_size, unsigned long *blocks)
{
    ataIOPB         pb;
    OSErr           status;
    long            slave;
    ATAPICmdPacket  cmdPacket;
    SCSI_10_Byte_Command *gReadCap;
    struct read_cap_data {
	long    addr;
	long    size;
    } rcd;

    clear_memory((void *)&pb, sizeof(pb));
    pb.ataPBFunctionCode    =   kATAMgrExecIO;
    pb.ataPBVers            =   kATAPBVers1;
    pb.ataPBDeviceID        =   deviceID;
    pb.ataPBFlags           =   mATAFlagTFRead | mATAFlagIORead | mATAFlagProtocol1;
    pb.ataPBTimeOut         =   kATAtimeout;

    pb.ataPBBuffer          =   (unsigned char *)&rcd;
    pb.ataPBByteCount = 8;
    pb.ataPBTaskFile.ataTFCylinder = 8;
    if (deviceID & 0x0FF00) {
	slave = 0x10;
    } else {
	slave = 0x0;
    }
			      /* std | L/C  | Drive | head */
    pb.ataPBTaskFile.ataTFSDH = 0xA0 | 0x40 | slave;
    pb.ataPBTaskFile.ataTFCommand = kATAcmdATAPIPacket;
    pb.ataPBPacketPtr = &cmdPacket;

    cmdPacket.atapiPacketSize = 16;
    clear_memory((void *)&cmdPacket.atapiCommandByte, 16);
    gReadCap = (SCSI_10_Byte_Command *) &cmdPacket.atapiCommandByte[0];

    gReadCap->opcode = kScsiCmdReadCapacity;


    status = ataManager((ataPB*) &pb );
    if (status != noErr) {
	/* failure */
	//printf("ATAPI read capacity status = %d\n", status);
	return 0;
    } else {
	*blocks = rcd.addr;
	*block_size = rcd.size;
	return 1;
    }
}


MEDIA
ATA_FindDevice(long dRefNum)
{
    ataDrvrRegister pb;
    OSErr       status;

    if (ATAManagerPresent()) {
	clear_memory((void *)&pb, sizeof(pb));
	
	pb.ataPBFunctionCode    =   kATAMgrFindDriverRefnum;
	pb.ataPBVers            =   kATAPBVers1;
	pb.ataPBDeviceID        =   0xFFFF;
	pb.ataPBTimeOut         =   kATAtimeout;
	
	pb.ataDeviceNextID = 1;
	do {
	    status = ataManager((ataPB*) &pb);
	
	    if (status != noErr) {
		break;
	    } else if (pb.ataDrvrRefNum == dRefNum
		    && pb.ataPBDeviceID != kNoDevice) {
		return open_ata_as_media(pb.ataPBDeviceID & 0xFF,
			(pb.ataPBDeviceID >> 8) & 0xFF);
	    } else {
		pb.ataPBDeviceID = pb.ataDeviceNextID;
	    }
	} while (pb.ataPBDeviceID != kNoDevice);
    }
    return 0;
}


#pragma mark -


ATA_MEDIA_ITERATOR
new_ata_iterator(void)
{
    return (ATA_MEDIA_ITERATOR) new_media_iterator(sizeof(struct ATA_media_iterator));
}


MEDIA_ITERATOR
create_ata_iterator(void)
{
    ATA_MEDIA_ITERATOR a;

    if (ata_inited == 0) {
	ata_init();
    }

    if (ata_mgr.exists == 0) {
	return 0;
    }

    a = new_ata_iterator();
    if (a != 0) {
	a->m.kind = ata_mgr.kind;
	a->m.state = kInit;
	a->m.do_reset = reset_ata_iterator;
	a->m.do_step = step_ata_iterator;
	a->m.do_delete = delete_ata_iterator;
	a->bus = 0;
	a->id = 0;
    }

    return (MEDIA_ITERATOR) a;
}


void
reset_ata_iterator(MEDIA_ITERATOR m)
{
    ATA_MEDIA_ITERATOR a;

    a = (ATA_MEDIA_ITERATOR) m;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != ata_mgr.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (a->m.state != kInit) {
	a->m.state = kReset;
    }
}


char *
step_ata_iterator(MEDIA_ITERATOR m)
{
    ATA_MEDIA_ITERATOR a;
    char *result;
    size_t len = 20;

    a = (ATA_MEDIA_ITERATOR) m;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != ata_mgr.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else {
	switch (a->m.state) {
	case kInit:
	    /* find # of buses (done in ata_init) */
	    a->m.state = kReset;
	    /* fall through to reset */
	case kReset:
	    a->bus = 0 /* low bus id */;
	    a->id = 0 /* low device id */;
	    a->m.state = kIterating;
	    /* fall through to iterate */
	case kIterating:
	    while (1) {
		if (a->bus >= ata_mgr.busCount/* max bus id */) {
		    break;
		}
		if (a->id > 1 /*max id for bus */) {
		    a->bus += 1;
		    a->id = 0 /* low device id */;
		    continue;   /* try again */
		}
		/* generate result */
		result = (char *) malloc(len);
		if (result != NULL) {
		    snprintf(result, len, "/dev/ata%c.%c", '0'+a->bus, '0'+a->id);
		}

		a->id += 1; /* next id */
		return result;
	    }
	    a->m.state = kEnd;
	    /* fall through to end */
	case kEnd:
	default:
	    break;
	}
    }
    return 0 /* no entry */;
}


void
delete_ata_iterator(MEDIA_ITERATOR m)
{
    return;
}


#pragma mark -


MEDIA
open_mklinux_ata_as_media(long index)
{
    long bus;
    long id;

    bus = index / 2;
    id = index % 2;

    return open_ata_as_media(bus, id);
}


char *
mklinux_ata_name(long bus, long id)
{
    char *result;
    size_t len = 20;

    result = (char *) malloc(len);
    if (result != NULL) {
    	/* name is hda, hdb, hdc, hdd, ...
    	 * in order (0,0)  (0,1)  (1,0)  (1,1) ...
    	 */
	snprintf(result, len, "/dev/hd%c", 'a' + (bus*2 + id));
    }
    return result;
}
