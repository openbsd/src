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
 *
 *   Project: ANGEL
 *
 *     Title: Definitions for device driver interface.
 */
#ifndef angsd_drivers_h
#define angsd_drivers_h

#include "rxtx.h"

#ifndef __cplusplus
typedef struct DeviceDescr DeviceDescr;
typedef struct DriverCall DriverCall;
#endif

/*
 * used to pass packets across the driver interface
 */
struct DriverCall
{
    struct data_packet  dc_packet;
    void               *dc_context;
};

/*
 * used to describe a device driver
 */
struct DeviceDescr
{
    char  *DeviceName;
    int  (*DeviceOpen)(const char *name, const char *arg);
    int  (*DeviceMatch)(const char *name, const char *arg);
    void (*DeviceClose)(void);
    int  (*DeviceRead)(DriverCall *dc, bool block);
    int  (*DeviceWrite)(DriverCall *dc);
    int  (*DeviceIoctl)(const int opcode, void *args);
    void  *SwitcherState;               /* used by switcher interface */
};

/*
 *  Function: DeviceOpen
 *
 *   Purpose: Open a communications device
 *
 *  Pre-conditions: No previous open is still active
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
 *   Returns:
 *          OK: 0
 *       Error: -1
 */
extern int DeviceOpen(const char *name, const char *arg);

/*
 *  Function: DeviceMatch
 *
 *   Purpose: Check whether parameters are OK to be passed to DeviceOpen
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
 *   Returns:
 *          OK: 0
 *       Error: -1
 */
extern int DeviceMatch(const char *name, const char *arg);

/*
 *  Function: DeviceClose
 *
 *   Purpose: Close a communications device
 *
 *  Pre-conditions: Device must have been previously opened
 *
 *    Params: None
 *
 *   Returns: Nothing
 */
extern void DeviceClose(void);

/*
 *  Function: DeviceRead
 *
 *   Purpose: Try to read a complete packet from a communications device.
 *              This read must usually be non-blocking, i.e. it should read as
 *              many data from the device as needed to complete the packet,
 *              but it should not wait if the packet is not complete, and no
 *              more data are currently available from the device.
 *            As an optimisation the read can optionally block when 'block'
 *              is TRUE, but only for a short time.  It is acceptable for the
 *              'block' parameter to be ignored in which case all reads
 *              should be non-blocking.
 *
 *  Pre-conditions: Device has been opened via DeviceOpen()
 *
 *    Params:
 *      In/Out: dc      Describes the packet being read (dc->dc_packet);
 *                      dc->dc_context is for the driver to store private
 *                      context, and is guaranteed to be NULL the first
 *                      time DeviceRead is called for a given packet.
 *
 *          In: block   If TRUE, read may safely block for a short period
 *                      of time (say up to 20ms), to avoid high CPU load
 *                      whilst waiting for a reply.
 *                      If FALSE, read MUST NOT block.
 *
 *   Returns:
 *          OK:  1 (packet is complete)
 *               0 (packet is not yet complete)
 *       Error: -1 bad packet
 *
 *   Post-conditions: should a calamatous error occur panic() will be called
 */
extern int DeviceRead(DriverCall *dc, bool block);

/*
 *  Function: DeviceWrite
 *
 *   Purpose: Try to write a packet to a communications device.  This write
 *              must be non-blocking, i.e. it should write as many data to
 *              the device as is immediately possible, but should not wait
 *              for space to send any more after that.
 *
 *  Pre-conditions: Device has been opened via DeviceOpen()
 *
 *    Params:
 *      In/Out: dc      Describes the packet being written (dc->dc_packet);
 *                      dc->dc_context is for the driver to store private
 *                      context, and is guaranteed to be NULL the first
 *                      time DeviceWrite is called for a given packet.
 *
 *   Returns:
 *          OK:  1 (all of the packet has been written)
 *               0 (some of the packet remains to be written)
 *       Error: -1
 */
extern int DeviceWrite(DriverCall *dc);

/*
 *  Function: DeviceIoctl
 *
 *   Purpose: Perform miscellaneous driver operations
 *
 *  Pre-conditions: Device has been open via DeviceOpen()
 *
 *    Params:
 *       Input: opcode  Reason code indicating the operation to perform
 *      In/Out: args    Pointer to opcode-sensitive arguments/result space
 *
 *   Returns:
 *          OK: 0
 *       Error: -1
 */
extern int DeviceIoctl(const int opcode, void *args);

#endif /* !defined(angsd_drivers_h) */

/* EOF drivers.h */
