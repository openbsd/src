/* 
 * Copyright (C) 1995 Advanced RISC Machines Limited. All rights reserved.
 * 
 * This software may be freely used, copied, modified, and distributed
 * provided that the above copyright notice is preserved in all copies of the
 * software.
 */

/* -*-C-*-
 *
 * $Revision: 1.3 $
 *     $Date: 2004/12/27 14:00:54 $
 *
 */
#ifndef angsd_devsw_h
#define angsd_devsw_h

#include "devclnt.h"
#include "adperr.h"
#include "drivers.h"

#ifndef __cplusplus
typedef struct Packet Packet;
typedef struct DevSWState DevSWState;
#endif

/*
 * the basic structure used for passing packets around
 */
struct Packet
{
    struct Packet *pk_next;             /* XXX first field in struct */
    unsigned int   pk_length;
    unsigned char *pk_buffer;
};

/*
 * control structure, used for maintaining device switcher state
 */
struct DevSWState
{
    unsigned int  ds_opendevchans;      /* bitmap of open device channels */

    /*
     * queue of packets read for the various device channels
     */
    Packet       *ds_readqueue[DC_NUM_CHANNELS];

    /*
     * structures for managing active read and write operations
     */
    Packet       *ds_nextreadpacket;
    DriverCall    ds_activeread;
    DriverCall    ds_activewrite;
};

#ifdef __cplusplus
    extern "C" {
#endif

/*
 *  Function: DevSW_AllocatePacket
 *   Purpose: Claim some memory to hold a struct Packet, and the buffer for
 *              that packet.
 *
 *    Params:
 *       Input: length  Size of the buffer in struct Packet.
 *
 *   Returns:
 *          OK: Pointer to the newly malloc()ed Packet.
 *       Error: NULL
 */
Packet *DevSW_AllocatePacket(const unsigned int length);

/*
 *  Function: DevSW_FreePacket
 *   Purpose: Free the memory associated with a struct Packet.
 *
 *  Pre-conditions The structure must have been originally claimed
 *                      via DevSW_AllocatePacket.
 *
 *    Params:
 *       Input: pk      The packet to be freed.
 *
 *   Returns: Nothing
 */
void DevSW_FreePacket(Packet *pk);

/*
 *  Function: DevSW_Open
 *   Purpose: Open the specified device driver
 *
 *    Params:
 *       Input: name    Identifies which device to open.  This can either be
 *                      a host specific identifier (e.g. "/dev/ttya",
 *                      "COM1:"), or a number which is used to refer to
 *                      `standard' interfaces, so "1" would be the first host
 *                      interface, "2" the second, and so on.
 *
 *              arg     Driver specific arguments.  For example, some serial
 *                      drivers accept speed and control arguments such as
 *                      "9600" or "19200/NO_BREAK".  These arguments are
 *                      completely free-form: it is the individual drivers
 *                      which do the necessary interpretation.
 *
 *              type    The type of packet the caller is interested in.  Only
 *                      one open is allowed for each type of packet.
 *
 *      In/Out: device  The device driver to open
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_open_failed
 *              adp_device_already_open
 *              adp_malloc_failure
 */
AdpErrs DevSW_Open(DeviceDescr *device, const char *name, const char *arg,
                   const DevChanID type);

/*
 *  Function: DevSW_Match
 *   Purpose: Minimal veneer for DeviceMatch
 *
 *    Params:
 *       Input: device  The device driver to match.
 *
 *              name    Identifies which device to open.  This can either be
 *                      a host specific identifier (e.g. "/dev/ttya",
 *                      "COM1:"), or a number which is used to refer to
 *                      `standard' interfaces, so "1" would be the first host
 *                      interface, "2" the second, and so on.
 *
 *              arg     Driver specific arguments.  For example, some serial
 *                      drivers accept speed and control arguments such as
 *                      "9600" or "19200/NO_BREAK".  These arguments are
 *                      completely free-form: it is the individual drivers
 *                      which do the necessary interpretation.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_failed
 */
AdpErrs DevSW_Match(const DeviceDescr *device, const char *name,
                    const char *arg);

/*
 *  Function: DevSW_Close
 *   Purpose: Close the specified device driver. All packets of the type
 *              used by the caller held within the switching layer will
 *              be discarded.
 *
 *  Pre-conditions: Device must have been previously opened.
 *
 *    Params:
 *       Input: device  The device driver to close
 *
 *              type    The type of packet the caller was interested in.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open
 */
AdpErrs DevSW_Close(DeviceDescr *device, const DevChanID type);

/*
 *  Function: DevSW_Read
 *   Purpose: Read a packet of appropriate type from the device driver
 *
 *    Params:
 *       Input: device  The device driver to read packet from.
 *
 *              type    The type of packet the caller is interested in.
 *
 *      Output: packet  Pointer to new packet (if one is available)
 *              NULL (if no complete packet is available)
 *
 *       Input: block   If TRUE, read may safely block for a short period
 *                      of time (say up to 20ms), to avoid high CPU load
 *                      whilst waiting for a reply.
 *                      If FALSE, read MUST NOT block.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_bad_packet
 *
 * Post-conditions: The calling function is responsible for freeing the
 *                      resources used by the packet when it is no longer
 *                      needed.
 */
AdpErrs DevSW_Read(const DeviceDescr *device, const DevChanID type,
                   Packet **packet, bool block);

/*
 *  Function: DevSW_Write
 *   Purpose: Try to write a packet to the device driver.  The write will
 *              be bounced if another write is still in progress.
 *
 *    Params:
 *       Input: device  The device driver to write a packet to.
 *
 *              packet  The packet to be written.
 *
 *              type    The type to be assigned to the packet.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_illegal_args
 *              adp_write_busy
 *
 * Post-conditions: The calling function retains "ownership" of the packet,
 *                      i.e. it is responsible for freeing the resources used
 *                      by the packet when it is no longer needed.
 */
AdpErrs DevSW_Write(const DeviceDescr *device, Packet *packet, DevChanID type);

/*
 *  Function: DevSW_FlushPendingWrite
 *   Purpose: If a write is in progress, give it a chance to finish.
 *
 *    Params:
 *       Input: device  The device driver to flush.
 *
 *   Returns:
 *              adp_ok           no pending write, or write flushed completely
 *              adp_write_busy   pending write not flushed completely
 */
AdpErrs DevSW_FlushPendingWrite(const DeviceDescr *device);

/*
 *  Function: DevSW_Ioctl
 *   Purpose: Perform miscellaneous control operations.  This is a minimal
 *              veneer to DeviceIoctl.
 *
 *    Params:
 *       Input: device  The device driver to control.
 *
 *              opcode  Reason code indicating the operation to perform.
 *
 *      In/Out: args    Pointer to opcode-sensitive arguments/result space.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_failed
 */
AdpErrs DevSW_Ioctl(const DeviceDescr *device, const int opcode, void *args);

/*
 *  Function: DevSW_WriteFinished
 *   Purpose: Return TRUE if the active device has finished writing
 *              the last packet to be sent, or FALSE if a packet is still
 *              being transmitted.
 *
 *    Params:
 *       Input: device  The device driver to check.
 *
 *   Returns:
 *        TRUE: write finished or inactive
 *       FALSE: write in progress
 */
bool DevSW_WriteFinished(const DeviceDescr *device);

      
/*
 * set filename and enable/disable logginf of ADP packets
 */
void DevSW_SetLogfile(const char *filename);
void DevSW_SetLogEnable(int logEnableFlag);
      
#ifdef __cplusplus
    }
#endif

#endif /* ndef angsd_devsw_h */

/* EOF devsw.h */
