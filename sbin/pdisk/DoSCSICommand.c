/*
 * DoScsiCommand.c
 *
 * This is the common entry to the original and asynchronous SCSI Manager calls:
 * if the asynchronous SCSI Manager is requested, it calls it. Otherwise, it
 * calls the original SCSI Manager and executes Request Sense if necessary.
 *
 * This function returns "autosense" in the SCSI_Sense_Data area. This will
 * be formatted in the senseMessage string.
 */

/*
 * Copyright 1992, 1993, 1997, 1998 by Apple Computer, Inc.
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

#include "DoScsiCommand.h"
#include "util.h"


//
// Defines
//
#define kSCSICommandTimeout     (5 * 1000L)         /* Five seconds             */
/*
 * This is the maximum number of times we try to grab the SCSI Bus
 */
#define kMaxSCSIRetries         40                  /* 10 seconds, 4 times/sec  */
/*
 * This test is TRUE if the SCSI bus status indicates "busy" (which is the case
 * if either the BSY or SEL bit is set).
 */
#ifndef kScsiStatBSY
#define kScsiStatBSY            (1 << 6)
#endif
#ifndef kScsiStatSEL
#define kScsiStatSEL            (1 << 1)
#endif
#define ScsiBusBusy()       ((SCSIStat() & (kScsiStatBSY | kScsiStatSEL)) != 0)


//
// Types
//


//
// Global Constants
//


//
// Global Variables
//
int             gSCSIHiBusID;
SCSIExecIOPB    *gSCSIExecIOPBPtr;
UInt32          gSCSIExecIOPBPtrLen;


//
// Forward declarations
//
UInt16 GetCommandLength(const SCSI_CommandPtr cmdPtr);
Boolean IsVirtualMemoryRunning(void);

OSErr OriginalSCSI(
    DeviceIdent             scsiDevice,
    const SCSI_CommandPtr   scsiCommand,
    UInt8                   scsiCommandLen,
    Ptr                     dataBuffer,
    ByteCount               dataLength,
    UInt32                  scsiFlags,
    ByteCount               *actualTransferCount,
    UInt8                   *scsiStatusByte
);

OSErr DoOriginalSCSICommand(
    DeviceIdent             scsiDevice,
    const SCSI_CommandPtr   theSCSICommand,
    unsigned short          cmdBlockLength,
    Ptr                     dataBuffer,
    ByteCount               dataLength,
    UInt32                  scsiFlags,
    ByteCount               *actualTransferCount,
    SCSI_Sense_Data         *sensePtr
);


//
// Routines
//

/*
 * This returns TRUE if the command failed with "Illegal Request." We need this
 * so we can ignore LogSense or ReadDefectData if the device doesn't support
 * these functions.
 */
Boolean
IsIllegalRequest(
    OSErr                   scsiStatus,
    const SCSI_Sense_Data   *senseDataPtr
    )
{
    Boolean                 result;
#define SENSE   (*senseDataPtr)

    result = FALSE;
    if (scsiStatus == scsiNonZeroStatus
     && (SENSE.senseKey & kScsiSenseKeyMask) == kScsiSenseIllegalReq
     && SENSE.additionalSenseLength >= 4) {
	switch ((SENSE.additionalSenseCode << 8) | SENSE.additionalSenseQualifier) {
	case 0x0000:
	case 0x2000:
	case 0x2022:    /* Obsolete */
	result = TRUE;
	break;
	default:
	break;
	}
    }
    return (result);
#undef SENSE
}


/*
 * This returns TRUE if the command failed with Device Not Ready (No Media Present)
 */
Boolean
IsNoMedia(
    OSErr                   scsiStatus,
    const SCSI_Sense_Data   *senseDataPtr
    )
{
    Boolean                 result;
#define SENSE   (*senseDataPtr)

    result = FALSE;
    if (scsiStatus == scsiNonZeroStatus
     && (SENSE.senseKey & kScsiSenseKeyMask) == kScsiSenseNotReady
     && SENSE.additionalSenseLength >= 4) {
	switch ((SENSE.additionalSenseCode << 8) | SENSE.additionalSenseQualifier) {
	case 0x0000:
	case 0x3A00:
	result = TRUE;
	break;
	default:
	break;
	}
    }
    return (result);
#undef SENSE
}


/*
 * Do one SCSI Command. If the device returns Check Condition, issue Request Sense
 * (original SCSI Manager only) and interpret the sense data. The original SCSI
 * command status is in SCB.status. If it is statusErr or scsiNonZeroStatus,
 * the sense data is in SCB.sense and the Request Sense status is in
 * SCB.requestSenseStatus.
 *
 * If sensePtr[0] is non-zero, there is a message.
 */
OSErr
DoSCSICommand(
    DeviceIdent             scsiDevice,
    ConstStr255Param        currentAction,
    const SCSI_CommandPtr   callerSCSICommand,
    Ptr                     dataBuffer,
    ByteCount               dataLength,
    UInt32                  scsiFlags,
    ByteCount               *actualTransferCount,
    SCSI_Sense_Data         *sensePtr,
    StringPtr               senseMessage
    )
{
    OSErr                   status;
    SCSI_Command            theSCSICommand;
    unsigned short          cmdBlockLength;
	
//      SpinSpinner(&gCurrentInfoPtr->spinnerRecord);
//      ShowProgressAction(currentAction);
    /*
     * Store the LUN information in the command block - this is needed
     * for devices that only examine the command block for LUN values.
     * (On SCSI-II, the asynchronous SCSI Manager also includes the
     * LUN in the identify message).
     */
    theSCSICommand = *callerSCSICommand;
    theSCSICommand.scsi[1] &= ~0xE0;
    theSCSICommand.scsi[1] |= (scsiDevice.LUN & 0x03) << 5;
    cmdBlockLength = GetCommandLength(&theSCSICommand);
    if (senseMessage != NULL)
	senseMessage[0] = 0;
    if (sensePtr != NULL)
	sensePtr->errorCode = 0;
    if (scsiDevice.bus == kOriginalSCSIBusAdaptor) {
	status = DoOriginalSCSICommand(
	    scsiDevice,
	    &theSCSICommand,
	    cmdBlockLength,
	    dataBuffer,
	    dataLength,
	    scsiFlags,
	    actualTransferCount,
	    sensePtr
	    );
    }
    else {
	clear_memory(gSCSIExecIOPBPtr, gSCSIExecIOPBPtrLen);
#define PB  (*gSCSIExecIOPBPtr)
	PB.scsiPBLength = gSCSIExecIOPBPtrLen;
	PB.scsiFunctionCode = SCSIExecIO;
	PB.scsiDevice = scsiDevice;
	PB.scsiTimeout = kSCSICommandTimeout;
	/*
	 * Fiddle the flags so they're the least disruptive possible.
	 */
	PB.scsiFlags = scsiFlags | (scsiSIMQNoFreeze | scsiDontDisconnect);
	if (sensePtr != NULL) {
	PB.scsiSensePtr = (UInt8 *) sensePtr;
	PB.scsiSenseLength = sizeof *sensePtr;
	}
	BlockMoveData(&theSCSICommand, &PB.scsiCDB.cdbBytes[0], cmdBlockLength);
	PB.scsiCDBLength = cmdBlockLength;
	if (dataBuffer != NULL) {
	PB.scsiDataPtr = (UInt8 *) dataBuffer;
	PB.scsiDataLength = dataLength;
	PB.scsiDataType = scsiDataBuffer;
	PB.scsiTransferType = scsiTransferPolled;
	}
	status = SCSIAction((SCSI_PB *) &PB);
	if (status == noErr)
	status = PB.scsiResult;
	if (status == scsiSelectTimeout)
	status = scsiDeviceNotThere;
	if (actualTransferCount != NULL) {
	/*
	 * Make sure that the actual transfer count does not exceed
	 * the allocation count (some devices spit extra data at us!)
	 */
	*actualTransferCount = dataLength - PB.scsiDataResidual;
	if (*actualTransferCount > dataLength)
	    *actualTransferCount = dataLength;
	}
#undef PB
    }
    if (status == scsiNonZeroStatus
     && sensePtr != NULL
     && sensePtr->errorCode != 0
     && senseMessage != NULL) {
//          FormatSenseMessage(sensePtr, senseMessage);
//          ShowProgressAction(senseMessage);
    }
    return (status);
}


/*
 * Do a command with autosense using the original SCSI manager.
 */
OSErr
DoOriginalSCSICommand(
    DeviceIdent             scsiDevice,
    const SCSI_CommandPtr   theSCSICommand,
    unsigned short          cmdBlockLength,
    Ptr                     dataBuffer,
    ByteCount               dataLength,
    UInt32                  scsiFlags,
    ByteCount               *actualTransferCount,
    SCSI_Sense_Data         *sensePtr
    )
{
    OSErr                   status;
    UInt8                   scsiStatusByte;
    SCSI_Command            scsiStatusCommand;

    status = OriginalSCSI(
	    scsiDevice,
	    theSCSICommand,
	    cmdBlockLength,
	    dataBuffer,
	    dataLength,
	    scsiFlags,
	    actualTransferCount,
	    &scsiStatusByte
	);
    if (status == scsiNonZeroStatus
     && scsiStatusByte == kScsiStatusCheckCondition
     && sensePtr != NULL) {
	CLEAR(scsiStatusCommand);
	CLEAR(*sensePtr);
	scsiStatusCommand.scsi6.opcode = kScsiCmdRequestSense;
	scsiStatusCommand.scsi[1] |= (scsiDevice.LUN & 0x03) << 5;
	scsiStatusCommand.scsi6.len = sizeof *sensePtr;
	status = OriginalSCSI(
	    scsiDevice,
	    &scsiStatusCommand,
	    sizeof scsiStatusCommand.scsi6,
	    (Ptr) sensePtr,
	    sizeof *sensePtr,
	    scsiDirectionIn,
	    NULL,
	    &scsiStatusByte
	    );
	if (status != noErr && status != scsiDataRunError) {
#ifdef notdef
	if (gDebugOnError && scsiStatusByte != kScsiStatusCheckCondition) {
	    Str255          work;

	    pstrcpy(work, "\pAutosense failed ");
	    AppendSigned(work, status);
	    AppendChar(work, ' ');
	    AppendHexLeadingZeros(work, scsiStatusByte, 2);
	    DebugStr(work);
	}
#endif
	sensePtr->errorCode = 0;
	status = scsiAutosenseFailed;
	}
	else {
	status = scsiNonZeroStatus;
	}
    }
    return (status);
}


OSErr
OriginalSCSI(
    DeviceIdent             scsiDevice,
    const SCSI_CommandPtr   scsiCommand,
    UInt8                   scsiCommandLen,
    Ptr                     dataBuffer,
    ByteCount               dataLength,
    UInt32                  scsiFlags,
    ByteCount               *actualTransferCount,
    UInt8                   *scsiStatusBytePtr
    )
{
    OSErr                   status;             /* Final status             */
    OSErr                   completionStatus;   /* Status from ScsiComplete */
    short                   totalTries;         /* Get/Select retries       */
    short                   getTries;           /* Get retries              */
    short                   iCount;             /* Bus free counter         */
    unsigned long           watchdog;           /* Timeout after this       */
    unsigned long           myTransferCount;    /* Gets TIB loop counter    */
    short                   scsiStatusByte;     /* Gets SCSIComplete result */
    short                   scsiMsgByte;        /* Gets SCSIComplete result */
    Boolean                 bufferHoldFlag;
    /*
     * The TIB has the following format:
     *  [0] scInc   user buffer         transferQuantum or transferSize
     *  [1] scAdd   &theTransferCount   1
     *  [2] scLoop  -> tib[0]           transferSize / transferQuantum
     *  [3] scStop
     * The intent of this is to return, in actualTransferCount, the number
     * of times we cycled through the tib[] loop. This will be the actual
     * transfer count if transferQuantum equals one, or the number of
     * "blocks" if transferQuantum is the length of one sector.
     */
    SCSIInstr               tib[4];             /* Current TIB              */

    status = noErr;
    bufferHoldFlag = FALSE;
    scsiStatusByte = 0xFF;
    scsiMsgByte = 0xFF;
    myTransferCount = 0;
    /*
     * If there is a data transfer, setup the tib.
     */
    if (dataBuffer != NULL) {
	tib[0].scOpcode = scInc;
	tib[0].scParam1 = (unsigned long) dataBuffer;
	tib[0].scParam2 = 1;
	tib[1].scOpcode = scAdd;
	tib[1].scParam1 = (unsigned long) &myTransferCount;
	tib[1].scParam2 = 1;
	tib[2].scOpcode = scLoop;
	tib[2].scParam1 = (-2 * sizeof (SCSIInstr));
	tib[2].scParam2 = dataLength / tib[0].scParam2;
	tib[3].scOpcode = scStop;
	tib[3].scParam1 = 0;
	tib[3].scParam2 = 0;
    }
    if (IsVirtualMemoryRunning() && dataBuffer != NULL) {
	/*
	 * Lock down the user buffer, if any. In a real-world application
	 * or driver, this would be done before calling the SCSI interface.
	 */
#ifdef notdef
	FailOSErr(
	HoldMemory(dataBuffer, dataLength),
	"\pCan't lock data buffer in physical memory"
	);
#else
	HoldMemory(dataBuffer, dataLength);
#endif
	bufferHoldFlag = TRUE;
    }
    /*
     * Arbitrate for the scsi bus.  This will fail if some other device is
     * accessing the bus at this time (which is unlikely).
     *
     *** Do not set breakpoints or call any functions that may require device
     *** I/O (such as display code that accesses font resources between
     *** SCSIGet and SCSIComplete,
     *
     */
    for (totalTries = 0; totalTries < kMaxSCSIRetries; totalTries++) {
	for (getTries = 0; getTries < 4; getTries++) {
	    /*
	     * Wait for the bus to go free.
	     */
	    watchdog = TickCount() + 300;       /* 5 second timeout         */
	    while (ScsiBusBusy()) {
		if (/*gStopNow || StopNow() ||*/ TickCount() > watchdog) {
		    status = scsiBusy;
		    goto exit;
		}
	    }
	    /*
	     * The bus is free, try to grab it
	     */
	    for (iCount = 0; iCount < 4; iCount++) {
		if ((status = SCSIGet()) == noErr)
		    break;
	    }
	    if (status == noErr) {
		break;                          /* Success: we have the bus */
	    }
	    /*
	     * The bus became busy again. Try to wait for it to go free.
	     */
	    for (iCount = 0;
		/*gStopNow == FALSE && StopNow() == FALSE &&*/ iCount < 100 && ScsiBusBusy();
		iCount++)
		;
	} /* The getTries loop */
	if (status != noErr) {
	    /*
	     * The SCSI Manager thinks the bus is not busy and not selected,
	     * but "someone" has set its internal semaphore that signals
	     * that the SCSI Manager itself is busy. The application will have
	     * to handle this problem. (We tried getTries * 4 times).
	     */
	    status = scsiBusy;
	    goto exit;
	}
	/*
	 * We now own the SCSI bus. Try to select the device.
	 */
	if ((status = SCSISelect(scsiDevice.targetID)) != noErr) {
	    switch (status) {
	    /*
	     * We get scBadParmsErr if we try to arbitrate for the initiator.
	     */
	    case scBadParmsErr: status = scsiTIDInvalid;        break;
	    case scCommErr:     status = scsiDeviceNotThere;    break;
	    case scArbNBErr:    status = scsiBusy;              break;
	    case scSequenceErr: status = scsiRequestInvalid;    break;
	    }
	    goto exit;
	}
	/*
	 * From this point on, we must exit through SCSIComplete() even if an
	 * error is detected. Send a command to the selected device. There are
	 * several failure modes, including an illegal command (such as a
	 * write to a read-only device). If the command failed because of
	 * "device busy", we will try it again.
	 */
	status = SCSICmd((Ptr) scsiCommand, scsiCommandLen);
	if (status != noErr) {
	    switch (status) {
	    case scCommErr:     status = scsiCommandTimeout;    break;
	    case scPhaseErr:    status = scsiSequenceFailed;    break;
	    }
	}
	if (status == noErr && dataBuffer != NULL) {
	    /*
	     * This command requires a data transfer.
	     */
	    if (scsiFlags == scsiDirectionOut) {
		status = SCSIWrite((Ptr) tib);
	    } else {
		status = SCSIRead((Ptr) tib);
	    }
	    switch (status) {
	    case scCommErr:     status = scsiCommandTimeout;        break;
	    case scBadParmsErr: status = scsiRequestInvalid;        break;
	    case scPhaseErr:    status = noErr; /* Don't care */    break;
	    case scCompareErr:                  /* Can't happen */  break;
	    }
	}
	/*
	 * SCSIComplete "runs" the bus-phase algorithm until the bitter end,
	 * returning the status and command-completion message bytes..
	 */
	completionStatus = SCSIComplete(
	    &scsiStatusByte,
	    &scsiMsgByte,
	    5 * 60L
	    );
	if (status == noErr && completionStatus != noErr) {
	    switch (completionStatus) {
	    case scCommErr:         status = scsiCommandTimeout;    break;
	    case scPhaseErr:        status = scsiSequenceFailed;    break;
	    case scComplPhaseErr:   status = scsiSequenceFailed;    break;
	    }
	}
	if (completionStatus == noErr && scsiStatusByte == kScsiStatusBusy) {
	    /*
	     * ScsiComplete is happy. If the device is busy,
	     * pause for 1/4 second and try again.
	     */
	    watchdog = TickCount() + 15;
	    while (TickCount() < watchdog)
		;
	    continue;               /* Do next totalTries attempt       */
	}
	/*
	 * This is the normal exit (success) or final failure exit.
	 */
	break;
    } /* totalTries loop */
exit:

    if (bufferHoldFlag) {
	(void) UnholdMemory(dataBuffer, dataLength);
    }
    /*
     * Return the number of bytes transferred to the caller. If the caller
     * supplied an actual count and the count is no greater than the maximum,
     * ignore any phase errors.
     */
    if (actualTransferCount != NULL) {
	*actualTransferCount = myTransferCount;
	if (*actualTransferCount > dataLength) {
	    *actualTransferCount = dataLength;
	}
    }
    /*
     * Also, there is a bug in the combination of System 7.0.1 and the 53C96
     * that may cause the real SCSI Status Byte to be in the Message byte.
     */
    if (scsiStatusByte == kScsiStatusGood
	    && scsiMsgByte == kScsiStatusCheckCondition) {
	scsiStatusByte = kScsiStatusCheckCondition;
    }
    if (status == noErr) {
	switch (scsiStatusByte) {
	case kScsiStatusGood:                               break;
	case kScsiStatusBusy:   status = scsiBusy;          break;
	case 0xFF:              status = scsiProvideFail;   break;
	default:                status = scsiNonZeroStatus; break;
	}
    }
    if (status == noErr
	    && (scsiFlags & scsiDirectionMask) != scsiDirectionNone
	    && myTransferCount != dataLength) {
	status = scsiDataRunError;
    }        
    if (scsiStatusBytePtr != NULL) {
	*scsiStatusBytePtr = scsiStatusByte;
    }
    return (status);
}


UInt16
GetCommandLength(
    const SCSI_CommandPtr   cmdPtr
    )
{
    unsigned short          result;
    /*
     * Look at the "group code" in the command operation. Return zero
     * error for the reserved (3, 4) and vendor-specific command (6, 7)
     * command groups. Otherwise, set the command length from the group code
     * value as specified in the SCSI-II spec.
     */
    switch (cmdPtr->scsi6.opcode & 0xE0) {
    case (0 << 5):  result = 6;     break;
    case (1 << 5):
    case (2 << 5):  result = 10;    break;
    case (5 << 5):  result = 12;    break;
    default:        result = 0;     break;
    }
    return (result);
}


Boolean
IsVirtualMemoryRunning(void)
{
    OSErr                       status;
    long                        response;
    
    status = Gestalt(gestaltVMAttr, &response);
    /*
     * VM is active iff Gestalt succeeded and the response is appropriate.
     */
    return (status == noErr && ((response & (1 << gestaltVMPresent)) != 0));
}


void
AllocatePB()
{
    OSErr           status;
    SCSIBusInquiryPB    busInquiryPB;
#define PB          (busInquiryPB)

    if (gSCSIExecIOPBPtr == NULL) {
	CLEAR(PB);
	PB.scsiPBLength = sizeof PB;
	PB.scsiFunctionCode = SCSIBusInquiry;
	PB.scsiDevice.bus = 0xFF;       /* Get info about the XPT */
	status = SCSIAction((SCSI_PB *) &PB);
	if (status == noErr)
	    status = PB.scsiResult;
	if (PB.scsiHiBusID == 0xFF) {
	    gSCSIHiBusID = -1;
	} else {
	    gSCSIHiBusID = PB.scsiHiBusID;
	}
	gSCSIExecIOPBPtrLen = PB.scsiMaxIOpbSize;
	if (gSCSIExecIOPBPtrLen != 0)
	    gSCSIExecIOPBPtr = (SCSIExecIOPB *) NewPtrClear(gSCSIExecIOPBPtrLen);
    }
#undef PB
}
