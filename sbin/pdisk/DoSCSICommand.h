/*
 * DoScsiCommand.h -
 *
 * Modified by Eryk Vershen (eryk@apple.com)
 * from an original by Martin Minow
 */

/*
 * Copyright 1993-1998 by Apple Computer, Inc.
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

#ifndef __DoScsiCommand__
#define __DoScsiCommand__

#include <SCSI.h>
#include "MacSCSICommand.h"


/*
 * Defines
 */
#ifndef EXTERN
#define EXTERN              extern
#endif

#ifndef TRUE
#define TRUE                1
#define FALSE               0
#endif

#ifndef NULL
#define NULL                0
#endif

#define kOriginalSCSIBusAdaptor (0xFF)

#define SameSCSIDevice(a, b) ((*((UInt32 *) &a)) == (*((UInt32 *) &b)))

/*
 * Cheap 'n dirty memory clear routine.
 */
#define CLEAR(dst)          clear_memory((void *) &dst, sizeof dst)


/*
 * Types
 */
#if !defined(__NewTypesDefined__)
#define __NewTypesDefined__
typedef signed char     SInt8;
typedef signed short    SInt16;
typedef signed long     SInt32;
typedef unsigned char   UInt8;
typedef unsigned short  UInt16;
typedef unsigned long   UInt32;
typedef unsigned long   ItemCount;
typedef unsigned long   ByteCount;
#endif


/*
 * Global Constants
 */
enum {
    bit0 = (1 << 0),
    bit1 = (1 << 1),
    bit2 = (1 << 2),
    bit3 = (1 << 3),
    bit4 = (1 << 4),
    bit5 = (1 << 5),
    bit6 = (1 << 6),
    bit7 = (1 << 7)
};


/*
 * Global Variables
 */
EXTERN int				gSCSIHiBusID;
EXTERN SCSIExecIOPB     *gSCSIExecIOPBPtr;
EXTERN UInt32           gSCSIExecIOPBPtrLen;


/*
 * Forward declarations
 */
void AllocatePB();
Boolean IsIllegalRequest(OSErr scsiStatus, const SCSI_Sense_Data *senseDataPtr);
Boolean IsNoMedia(OSErr scsiStatus, const SCSI_Sense_Data *senseDataPtr);
/*
 * All SCSI Commands come here.
 *  if scsiDevice.busID == kOriginalSCSIBusAdaptor, IM-IV SCSI will be called.
 *  scsiFlags should be scsiDirectionNone, scsiDirectionIn, or scsiDirectionOut
 *  actualTransferCount may be NULL if you don't care.
 *  Both old and new SCSI return SCSI Manager 4.3 errors.
 *
 * DoSCSICommand throws really serious errors, but returns SCSI errors such
 * as dataRunError and scsiDeviceNotThere.
 */
OSErr DoSCSICommand(
	DeviceIdent             scsiDevice,
	ConstStr255Param        currentAction,
	const SCSI_CommandPtr   callerSCSICommand,
	Ptr                     dataBuffer,
	ByteCount               dataLength,
	UInt32                  scsiFlags,
	ByteCount               *actualTransferCount,
	SCSI_Sense_Data         *sensePtr,
	StringPtr               senseMessage
);


#endif /* __DoScsiCommand__ */
