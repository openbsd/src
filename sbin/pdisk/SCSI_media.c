/*
 * SCSI_media.c -
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


#include <stdio.h>
#include <stdlib.h>
#include "DoSCSICommand.h"
#include "SCSI_media.h"
#include "util.h"


/*
 * Defines
 */
#define DriverRefNumToSCSI(x)  ((signed short) (~(x) - 32))


/*
 * Types
 */
typedef struct SCSI_media *SCSI_MEDIA;

struct SCSI_media {
    struct media    m;
    long            bus;
    long            id;
};

struct bus_entry {
    long    bus;
    long    sort_value;
    long    max_id;
    long    master_id;
};

struct SCSI_manager {
    long        exists;
    long        kind;
    long        bus_count;
    struct bus_entry *bus_list;
};

typedef struct SCSI_media_iterator *SCSI_MEDIA_ITERATOR;

struct SCSI_media_iterator {
    struct media_iterator   m;
    long                    bus_index;
    long                    bus;
    long                    id;
};

struct mklinux_order_cache {
    struct cache_item *first;
    struct cache_item *last;
    long next_disk;
    long next_cdrom;
    long loaded;
};

struct cache_item {
    struct cache_item *next;
    long bus;
    long id;
    long value;
    long is_cdrom;
    long unsure;
};


/*
 * Global Constants
 */
enum {
    kNoDevice = 0x00FF
};

enum {
    kRequiredSCSIinquiryLength = 36
};


/*
 * Global Variables
 */
static long scsi_inited = 0;
static struct SCSI_manager scsi_mgr;
static struct mklinux_order_cache mklinux_order;


/*
 * Forward declarations
 */
int AsyncSCSIPresent(void);
void scsi_init(void);
SCSI_MEDIA new_scsi_media(void);
long read_scsi_media(MEDIA m, long long offset, unsigned long count, void *address);
long write_scsi_media(MEDIA m, long long offset, unsigned long count, void *address);
long close_scsi_media(MEDIA m);
long os_reload_scsi_media(MEDIA m);
long compute_id(long bus, long device);
int SCSI_ReadBlock(UInt32 id, UInt32 bus, UInt32 block_size, UInt32 block, UInt8 *address);
int SCSI_WriteBlock(UInt32 id, UInt32 bus, UInt32 block_size, UInt32 block, UInt8 *address);
int DoTestUnitReady(UInt8 targetID, int bus);
int DoReadCapacity(UInt32 id, UInt32 bus, UInt32 *blockCount, UInt32 *blockSize);
SCSI_MEDIA_ITERATOR new_scsi_iterator(void);
void reset_scsi_iterator(MEDIA_ITERATOR m);
char *step_scsi_iterator(MEDIA_ITERATOR m);
void delete_scsi_iterator(MEDIA_ITERATOR m);
void fill_bus_entry(struct bus_entry *entry, long bus);
/*long get_bus_sort_value(long bus);*/
int bus_entry_compare(const void* a, const void* b);
int DoInquiry(UInt32 id, UInt32 bus, UInt32 *devType);
void probe_all(void);
void probe_scsi_device(long bus, long id, int unsure);
long lookup_scsi_device(long bus, long id, int *is_cdrom, int *unsure);
long lookup_scsi_index(long index, int is_cdrom, long *bus, long *id);
void add_to_cache(long bus, long id, int is_cdrom, int unsure);
void init_mklinux_cache(void);
void clear_mklinux_cache(void);
void mark_mklinux_cache_loaded(void);
int mklinux_cache_loaded(void);


/*
 * Routines
 */
int
AsyncSCSIPresent(void)
{
    return (TrapAvailable(_SCSIAtomic));
}


void
scsi_init(void)
{
    int i;
    int old_scsi;

    if (scsi_inited != 0) {
	return;
    }
    scsi_inited = 1;

    scsi_mgr.exists = 1;
    scsi_mgr.kind = allocate_media_kind();

    if (AsyncSCSIPresent()) {
	AllocatePB();

	scsi_mgr.bus_count = gSCSIHiBusID + 1;
	old_scsi = 0;
    } else {
	scsi_mgr.bus_count = 1;
	old_scsi = 1;
    }

    scsi_mgr.bus_list = (struct bus_entry *)
	    calloc(scsi_mgr.bus_count, sizeof(struct bus_entry));
	
    if (scsi_mgr.bus_list == 0) {
	scsi_mgr.bus_count = 0;
    } else {
	for (i = 0; i < scsi_mgr.bus_count; i++) {
	    if (old_scsi) {
		scsi_mgr.bus_list[i].bus = 0xFF;
	    } else {
		scsi_mgr.bus_list[i].bus = i;
	    }
	    fill_bus_entry(&scsi_mgr.bus_list[i], i);
	}
	qsort((void *)scsi_mgr.bus_list,    /* address of array */
		scsi_mgr.bus_count,         /* number of elements */
		sizeof(struct bus_entry),   /* size of element */
		bus_entry_compare);         /* element comparison routine */
    }

    init_mklinux_cache();
}

void
fill_bus_entry(struct bus_entry *entry, long bus)
{
    OSErr           status;
    SCSIBusInquiryPB    pb;
    long len;
    long result;
    long x, y;

    if (!AsyncSCSIPresent()) {
    	entry->sort_value = 0;
	entry->max_id = 7;
	entry->master_id = 7;
	return;
    }
    len = sizeof(SCSIBusInquiryPB);
    clear_memory((Ptr) &pb, len);
    pb.scsiPBLength = len;
    pb.scsiFunctionCode = SCSIBusInquiry;
    pb.scsiDevice.bus = bus;
    status = SCSIAction((SCSI_PB *) &pb);
    if (status != noErr) {
	result = 6;
    } else {
	switch (pb.scsiHBAslotType) {
	case scsiMotherboardBus:    x = 0; break;
	case scsiPDSBus:            x = 1; break;
	case scsiNuBus:             x = 2; break;
	case scsiPCIBus:            x = 3; break;
	case scsiFireWireBridgeBus: x = 4; break;
	case scsiPCMCIABus:         x = 5; break;
	default:                    x = 7 + pb.scsiHBAslotType; break;
	};
	
	switch (pb.scsiFeatureFlags & scsiBusInternalExternalMask) {
	case scsiBusInternal:                   y = 0; break;
	case scsiBusInternalExternal:           y = 1; break;
	case scsiBusExternal:                   y = 2; break;
	default:
	case scsiBusInternalExternalUnknown:    y = 3; break;
	};
	result = x * 4 + y;
    }
    entry->sort_value = result;
    entry->max_id = pb.scsiMaxLUN;
    entry->master_id = pb.scsiInitiatorID;
}


int
bus_entry_compare(const void* a, const void* b)
{
    long result;

    const struct bus_entry *x = (const struct bus_entry *) a;
    const struct bus_entry *y = (const struct bus_entry *) b;

    result = x->sort_value - y->sort_value;
    if (result == 0) {
	result = x->bus - y->bus;
    }
    return result;
}


SCSI_MEDIA
new_scsi_media(void)
{
    return (SCSI_MEDIA) new_media(sizeof(struct SCSI_media));
}


MEDIA
open_old_scsi_as_media(long device)
{
    return open_scsi_as_media(kOriginalSCSIBusAdaptor, device);
}


MEDIA
open_scsi_as_media(long bus, long device)
{
    SCSI_MEDIA  a;
    UInt32 blockCount;
    UInt32 blockSize;

    if (scsi_inited == 0) {
	scsi_init();
    }

    if (scsi_mgr.exists == 0) {
	return 0;
    }

    a = 0;
    if (DoTestUnitReady(device, bus) > 0) {
	if (DoReadCapacity(device, bus, &blockCount, &blockSize) != 0) {
	    a = new_scsi_media();
	    if (a != 0) {
		a->m.kind = scsi_mgr.kind;
		a->m.grain = blockSize;
		a->m.size_in_bytes = ((long long)blockCount) * blockSize;
		a->m.do_read = read_scsi_media;
		a->m.do_write = write_scsi_media;
		a->m.do_close = close_scsi_media;
		a->m.do_os_reload = os_reload_scsi_media;
		a->bus = bus;
		a->id = device;
	    }
	}
    }
    return (MEDIA) a;
}


long
read_scsi_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    SCSI_MEDIA a;
    long rtn_value;
    long block;
    long block_count;
    long block_size;
    unsigned char *buffer;
    int i;

    block = (long) offset;
//printf("scsi %d count %d\n", block, count);
    a = (SCSI_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != scsi_mgr.kind) {
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
	    if (SCSI_ReadBlock(a->id, a->bus, block_size, block, buffer) == 0) {
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
write_scsi_media(MEDIA m, long long offset, unsigned long count, void *address)
{
    SCSI_MEDIA a;
    long rtn_value;
    long block;
    long block_count;
    long block_size;
    unsigned char *buffer;
    int i;

    a = (SCSI_MEDIA) m;
    rtn_value = 0;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != scsi_mgr.kind) {
	/* XXX need to error here - this is an internal problem */
    } else if (count <= 0 || count % a->m.grain != 0) {
	/* can't handle size */
    } else if (offset < 0 || offset % a->m.grain != 0) {
	/* can't handle offset */
    } else if (offset + count > a->m.size_in_bytes) {
	/* check for offset (and offset+count) too large */
    } else {
	/* XXX do a write on the physical device */
	block_size = a->m.grain;
	block = offset / block_size;
	block_count = count / block_size;
	buffer = address;
	rtn_value = 1;
	for (i = 0; i < block_count; i++) {
	    if (SCSI_WriteBlock(a->id, a->bus, block_size, block, buffer) == 0) {
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
close_scsi_media(MEDIA m)
{
    SCSI_MEDIA a;

    a = (SCSI_MEDIA) m;
    if (a == 0) {
	return 0;
    } else if (a->m.kind != scsi_mgr.kind) {
	/* XXX need to error here - this is an internal problem */
	return 0;
    }
    /* XXX nothing to do - I think? */
    return 1;
}


long
os_reload_scsi_media(MEDIA m)
{
    printf("Reboot your system so the partition table will be reread.\n");
    return 1;
}


#pragma mark -


int
DoTestUnitReady(UInt8 targetID, int bus)
{
    OSErr                   status;
    Str255                  errorText;
    char*       msg;
    static const SCSI_6_Byte_Command gTestUnitReadyCommand = {
	kScsiCmdTestUnitReady, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data         senseData;
    DeviceIdent scsiDevice;
    int rtn_value;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = bus;
    scsiDevice.targetID = targetID;
    scsiDevice.LUN = 0;

    status = DoSCSICommand(
		scsiDevice,
		"\pTest Unit Ready",
		(SCSI_CommandPtr) &gTestUnitReadyCommand,
		NULL,
		0,
		scsiDirectionNone,
		NULL,
		&senseData,
		errorText
		);
    if (status == scsiNonZeroStatus) {
	rtn_value = -1;
    } else if (status != noErr) {
	rtn_value = 0;
    } else {
	rtn_value = 1;
    }
    return rtn_value;
}


int
SCSI_ReadBlock(UInt32 id, UInt32 bus, UInt32 block_size, UInt32 block, UInt8 *address)
{
    OSErr                   status;
    Str255                  errorText;
    char*       msg;
    static SCSI_10_Byte_Command gReadCommand = {
	kScsiCmdRead10, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data         senseData;
    DeviceIdent scsiDevice;
    int rtn_value;
    long count;

//printf("scsi read %d:%d block %d size %d\n", bus, id, block, block_size);
    scsiDevice.diReserved = 0;
    scsiDevice.bus = bus;
    scsiDevice.targetID = id;
    scsiDevice.LUN = 0;

    gReadCommand.lbn4 = (block >> 24) & 0xFF;
    gReadCommand.lbn3 = (block >> 16) & 0xFF;
    gReadCommand.lbn2 = (block >> 8) & 0xFF;
    gReadCommand.lbn1 = block & 0xFF;

    count = 1;
    gReadCommand.len2 = (count >> 8) & 0xFF;
    gReadCommand.len1 = count & 0xFF;

    status = DoSCSICommand(
		scsiDevice,
		"\pRead",
		(SCSI_CommandPtr) &gReadCommand,
		(Ptr) address,
		count * block_size,
		scsiDirectionIn,
		NULL,
		&senseData,
		errorText
	);
    if (status == noErr) {
	rtn_value = 1;
    } else {
	rtn_value = 0;
    }
    return rtn_value;
}


int
SCSI_WriteBlock(UInt32 id, UInt32 bus, UInt32 block_size, UInt32 block, UInt8 *address)
{
    OSErr                   status;
    Str255                  errorText;
    char*       msg;
    static SCSI_10_Byte_Command gWriteCommand = {
	kScsiCmdWrite10, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data         senseData;
    DeviceIdent scsiDevice;
    int rtn_value;
    long count;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = bus;
    scsiDevice.targetID = id;
    scsiDevice.LUN = 0;

    gWriteCommand.lbn4 = (block >> 24) & 0xFF;
    gWriteCommand.lbn3 = (block >> 16) & 0xFF;
    gWriteCommand.lbn2 = (block >> 8) & 0xFF;
    gWriteCommand.lbn1 = block & 0xFF;

    count = 1;
    gWriteCommand.len2 = (count >> 8) & 0xFF;
    gWriteCommand.len1 = count & 0xFF;

    status = DoSCSICommand(
		scsiDevice,
		"\pWrite",
		(SCSI_CommandPtr) &gWriteCommand,
		(Ptr) address,
		count * block_size,
		scsiDirectionOut,
		NULL,
		&senseData,
		errorText
	);
    if (status == noErr) {
	rtn_value = 1;
    } else {
	rtn_value = 0;
    }
    return rtn_value;
}


int
DoReadCapacity(UInt32 id, UInt32 bus, UInt32 *blockCount, UInt32 *blockSize)
{
    OSErr       status;
    Str255      errorText;
    static const SCSI_10_Byte_Command gCapacityCommand = {
	kScsiCmdReadCapacity, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };
    SCSI_Sense_Data senseData;
    DeviceIdent     scsiDevice;
    SCSI_Capacity_Data  capacityData;
    UInt32      temp;
    int rtn_value;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = bus;
    scsiDevice.targetID = id;
    scsiDevice.LUN = 0;

    CLEAR(capacityData);

    status = DoSCSICommand(
		scsiDevice,
		"\pRead Capacity",
		(SCSI_CommandPtr) &gCapacityCommand,
		(Ptr) &capacityData,
		sizeof (SCSI_Capacity_Data),
		scsiDirectionIn,
		NULL,
		&senseData,
		errorText
		);

    if (status == noErr) {
	temp = capacityData.lbn4;
	temp = (temp << 8) | capacityData.lbn3;
	temp = (temp << 8) | capacityData.lbn2;
	temp = (temp << 8) | capacityData.lbn1;
	*blockCount = temp;

	temp = capacityData.len4;
	temp = (temp << 8) | capacityData.len3;
	temp = (temp << 8) | capacityData.len2;
	temp = (temp << 8) | capacityData.len1;
	*blockSize = temp;

	rtn_value = 1;
    } else {
	rtn_value = 0;
    }
    return rtn_value;
}


int
DoInquiry(UInt32 id, UInt32 bus, UInt32 *devType)
{
    OSErr       status;
    Str255      errorText;
    static const SCSI_6_Byte_Command gInquiryCommand = {
	kScsiCmdInquiry, 0, 0, 0, kRequiredSCSIinquiryLength, 0
    };
    SCSI_Sense_Data senseData;
    DeviceIdent     scsiDevice;
    SCSI_Inquiry_Data  inquiryData;
    UInt32      temp;
    int rtn_value;

    scsiDevice.diReserved = 0;
    scsiDevice.bus = bus;
    scsiDevice.targetID = id;
    scsiDevice.LUN = 0;

    CLEAR(inquiryData);

    status = DoSCSICommand(
		scsiDevice,
		"\pInquiry",
		(SCSI_CommandPtr) &gInquiryCommand,
		(Ptr) &inquiryData,
		kRequiredSCSIinquiryLength,
		scsiDirectionIn,
		NULL,
		&senseData,
		errorText
		);

    if (status == noErr) {
	*devType = inquiryData.devType & kScsiDevTypeMask;
	rtn_value = 1;
    } else {
	rtn_value = 0;
    }
    return rtn_value;
}


MEDIA
SCSI_FindDevice(long dRefNum)
{
    SCSIDriverPB            pb;
    OSErr                   status;
    short                   targetID;

    status = nsvErr;
    if (AsyncSCSIPresent()) {
	clear_memory((Ptr) &pb, sizeof pb);
	
	pb.scsiPBLength = sizeof (SCSIDriverPB);
	pb.scsiCompletion = NULL;
	pb.scsiFlags = 0;
	pb.scsiFunctionCode = SCSILookupRefNumXref;
	pb.scsiDevice.bus = kNoDevice;  /* was *((long *) &pb.scsiDevice) = 0xFFFFFFFFL; */
	
	do {
	    status = SCSIAction((SCSI_PB *) &pb);
	
	    if (status != noErr) {
		break;
	    } else if (pb.scsiDriver == dRefNum
		    && pb.scsiDevice.bus != kNoDevice) {
		return open_scsi_as_media(pb.scsiDevice.bus, pb.scsiDevice.targetID);

	    } else {
		pb.scsiDevice = pb.scsiNextDevice;
	    }
	}
	while (pb.scsiDevice.bus != kNoDevice);
    }
    if (status == nsvErr) {
	/*
	 * The asynchronous SCSI Manager is missing or the
	 * driver didn't register with the SCSI Manager.*/
	targetID = DriverRefNumToSCSI(dRefNum);
	if (targetID >= 0 && targetID <= 6) {
	    return open_old_scsi_as_media(targetID);
	}
    }
     return 0;
}


#pragma mark -


SCSI_MEDIA_ITERATOR
new_scsi_iterator(void)
{
    return (SCSI_MEDIA_ITERATOR) new_media_iterator(sizeof(struct SCSI_media_iterator));
}


MEDIA_ITERATOR
create_scsi_iterator(void)
{
    SCSI_MEDIA_ITERATOR a;

    if (scsi_inited == 0) {
	scsi_init();
    }

    if (scsi_mgr.exists == 0) {
	return 0;
    }

    a = new_scsi_iterator();
    if (a != 0) {
	a->m.kind = scsi_mgr.kind;
	a->m.state = kInit;
	a->m.do_reset = reset_scsi_iterator;
	a->m.do_step = step_scsi_iterator;
	a->m.do_delete = delete_scsi_iterator;
	a->bus_index = 0;
	a->bus = 0;
	a->id = 0;
    }

    return (MEDIA_ITERATOR) a;
}


void
reset_scsi_iterator(MEDIA_ITERATOR m)
{
    SCSI_MEDIA_ITERATOR a;

    a = (SCSI_MEDIA_ITERATOR) m;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != scsi_mgr.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else if (a->m.state != kInit) {
	a->m.state = kReset;
    }
}


char *
step_scsi_iterator(MEDIA_ITERATOR m)
{
    SCSI_MEDIA_ITERATOR a;
    char *result;
    size_t len = 20;

    a = (SCSI_MEDIA_ITERATOR) m;
    if (a == 0) {
	/* no media */
    } else if (a->m.kind != scsi_mgr.kind) {
	/* wrong kind - XXX need to error here - this is an internal problem */
    } else {
	switch (a->m.state) {
	case kInit:
	    /* find # of buses - done in AllocatePB() out of scsi_init() */
	    a->m.state = kReset;
	    /* fall through to reset */
	case kReset:
	    a->bus_index = 0 /* first bus id */;
	    a->bus = scsi_mgr.bus_list[a->bus_index].bus;
	    a->id = 0 /* first device id */;
	    a->m.state = kIterating;
	    clear_mklinux_cache();
	    /* fall through to iterate */
	case kIterating:
	    while (1) {
		if (a->bus_index >= scsi_mgr.bus_count /* max bus id */) {
		    break;
		}
		if (a->id == scsi_mgr.bus_list[a->bus_index].master_id) {
		    /* next id */
		    a->id += 1;
		}
		if (a->id > scsi_mgr.bus_list[a->bus_index].max_id) {
		    a->bus_index += 1;
		    a->bus = scsi_mgr.bus_list[a->bus_index].bus;
		    a->id = 0 /* first device id */;
		    continue;   /* try again */
		}
		/* generate result */
		result = (char *) malloc(len);
		if (result != NULL) {
		    if (a->bus == 0xFF) {
			snprintf(result, len, "/dev/scsi%c", '0'+a->id);
			probe_scsi_device(a->bus, a->id, 1);
		    } else {
			snprintf(result, len, "/dev/scsi%c.%c", '0'+a->bus, '0'+a->id);
			/* only probe out of iterate; so always added in order. */
			probe_scsi_device(a->bus, a->id, 0);
		    }
		}

		a->id += 1; /* next id */
		return result;
	    }
	    a->m.state = kEnd;
	    /* fall through to end */
	case kEnd:
	    mark_mklinux_cache_loaded();
	default:
	    break;
	}
    }
    return 0 /* no entry */;
}


void
delete_scsi_iterator(MEDIA_ITERATOR m)
{
    return;
}


#pragma mark -


MEDIA
open_mklinux_scsi_as_media(long index, int is_cdrom)
{
    MEDIA m;
    long bus;
    long id;

    if (lookup_scsi_index(index, is_cdrom, &bus, &id) > 0) {
	m = open_scsi_as_media(bus, id);
    } else {
	m = 0;
    }

    return m;
}


char *
mklinux_old_scsi_name(long id)
{
    mklinux_scsi_name(kOriginalSCSIBusAdaptor, id);
}


char *
mklinux_scsi_name(long bus, long id)
{
    char *result = 0;
    long value;
    int is_cdrom;
    int unsure;
    char *suffix;
    size_t len = 20;

    /* name is sda, sdb, sdc, ...
     * in order by buses and ids, but only count responding devices ...
     */
    if ((value = lookup_scsi_device(bus, id, &is_cdrom, &unsure)) >= 0) {
	result = (char *) malloc(len);
	if (result != NULL) {
	    if (unsure) {
		suffix = " ?";
	    } else {
		suffix = "";
	    }
	    if (is_cdrom) {
		snprintf(result, len, "/dev/scd%c%s", '0' + value, suffix);
	    } else {
		snprintf(result, len, "/dev/sd%c%s", 'a' + value, suffix);
	    }
	}
    }
    return result;
}


void
probe_all(void)
{
    MEDIA_ITERATOR iter;
    char *name;

    iter = create_scsi_iterator();
    if (iter == 0) {
	return;
    }

    printf("finding devices ");
    fflush(stdout);
    while ((name = step_media_iterator(iter)) != 0) {
    	/* step does the probe for us */
	printf(".");
	fflush(stdout);
	free(name);
    }
    delete_media_iterator(iter);
    printf("\n");
    fflush(stdout);
}


void
probe_scsi_device(long bus, long id, int unsure)
{
    UInt32 devType;

    if (DoInquiry(id, bus, &devType)) {
    	if (devType == kScsiDevTypeDirect || devType == kScsiDevTypeWorm
    		|| devType == kScsiDevTypeOptical) {
    	    add_to_cache(bus, id, 0, unsure);
    	} else if (devType == kScsiDevTypeCDROM) {
    	    add_to_cache(bus, id, 1, unsure);
    	}
    }
}


long
lookup_scsi_device(long bus, long id, int *is_cdrom, int *unsure)
{
    /* walk down list looking for bus and id ?
     *
     * only probe out of iterate (so always add in order)
     * reset list if we reset the iterate
     */
    struct cache_item *item;
    struct cache_item *next;
    long result = -1;
    int count = 0;

    if (scsi_inited == 0) {
	scsi_init();
    }

    while (1) {
    	count++;
	for (item = mklinux_order.first; item != NULL; item = item->next) {
	    if (item->bus == bus && item->id == id) {
		result = item->value;
		*is_cdrom = item->is_cdrom;
		*unsure = item->unsure;
		break;
	    }
	}
	if (count < 2 && result < 0) {
	    probe_all();
	} else {
	    break;
	}
    };

    return result;
}


/*
 * This has the same structure as lookup_scsi_device() except we are
 * matching on the value & type rather than the bus & id.
 */
long
lookup_scsi_index(long index, int is_cdrom, long *bus, long *id)
{
    struct cache_item *item;
    struct cache_item *next;
    long result = 0;
    int count = 0;

    if (scsi_inited == 0) {
	scsi_init();
    }

    while (1) {
    	count++;
	for (item = mklinux_order.first; item != NULL; item = item->next) {
	    if (item->value == index && item->is_cdrom == is_cdrom
		    && item->unsure == 0) {
		result = 1;
		*bus = item->bus;
		*id = item->id;
		break;
	    }
	}
	if (count < 2 && result == 0 && !mklinux_cache_loaded()) {
	    probe_all();
	} else {
	    break;
	}
    };

    return result;
}


void
add_to_cache(long bus, long id, int is_cdrom, int unsure)
{
    struct cache_item *item;

    item = malloc(sizeof(struct cache_item));
    if (item == NULL) {
	return;
    } else {
	item->bus = bus;
	item->id = id;
	item->is_cdrom = is_cdrom;
	item->unsure = unsure;
	if (is_cdrom) {
	    item->value = mklinux_order.next_cdrom;
	    mklinux_order.next_cdrom++;
	} else {
	    item->value = mklinux_order.next_disk;
	    mklinux_order.next_disk++;
	}
	item->next = 0;
    }
    if (mklinux_order.first == NULL) {
	mklinux_order.first = item;
	mklinux_order.last = item;
    } else {
	mklinux_order.last->next = item;
	mklinux_order.last = item;
    }
}


void
init_mklinux_cache(void)
{
    mklinux_order.first = NULL;
    clear_mklinux_cache();
}


void
clear_mklinux_cache(void)
{
    struct cache_item *item;
    struct cache_item *next;

    for (item = mklinux_order.first; item != NULL; item = next) {
	next = item->next;
	free(item);
    }
    /* back to starting value */
    mklinux_order.first = NULL;
    mklinux_order.last = NULL;
    mklinux_order.next_disk = 0;
    mklinux_order.next_cdrom = 0;
    mklinux_order.loaded = 0;
}


void
mark_mklinux_cache_loaded(void)
{
    mklinux_order.loaded = 1;
}


int
mklinux_cache_loaded(void)
{
    return mklinux_order.loaded;
}
