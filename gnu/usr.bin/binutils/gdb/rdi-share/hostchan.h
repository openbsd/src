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
#ifndef angsd_hostchan_h
#define angsd_hostchan_h

/* A temporary sop to older compilers */
#if defined (__NetBSD__) || defined (unix)
#  ifndef __unix              /* (good for long-term portability?)  */
#    define __unix    1
#  endif
#endif

/* struct timeval */
#if defined(__unix) || defined(__CYGWIN__)
#  include <sys/time.h>
#else
#  include "winsock.h"
#  include "time.h"
#endif

#include "chandefs.h"
#include "adperr.h"
#include "devsw.h"

/*
 * asynchronous processing modes
 */
enum AsyncMode
{
    async_block_on_nothing,
    async_block_on_read,
    async_block_on_write
};

#ifndef __cplusplus
typedef enum AsyncMode AsyncMode;
#endif

/*
 * prototype for channels callback function
 */
typedef void (*ChannelCallback)(Packet *packet, void *state);

/*
 *  Function: Adp_initSeq
 *   Purpose: initialise the channel protocol and sequence numbers
 *
 *    Params: none
 *
 *   Returns: Nothing
 */
extern void Adp_initSeq(void);

/*
 *  Function: Adp_addToQueue
 *   Purpose: chain a Packet to the end of a linked list of such structures
 *
 *    Params:
 *      In/Out: head    Head of the linked list
 *
 *              newpkt  Packet to be chained onto the list
 *
 *   Returns: Nothing
 */
extern void Adp_addToQueue(Packet **head, Packet *newpkt);

/*
 *  Function: removeFromQueue
 *   Purpose: remove a Packet from the head of a linked list of such structures
 *
 *    Params:
 *      In/Out: head    Head of the linked list
 *
 *   Returns: Old head from the linked list
 *
 * Post-conditions: Second element in the list will be the new head.
 */

extern Packet *Adp_removeFromQueue(Packet **head);

/*
 * Set log file and Enable/disable logging of ADP packets to file.
 */

void Adp_SetLogfile(const char *filename);
void Adp_SetLogEnable(int logEnableFlag);

/*
 *  Function: Adp_OpenDevice
 *   Purpose: Open a device to use for channels communication.  This is a
 *              very thin veneer to the device drivers: what hostchan.c
 *              will do is call DeviceMatch for each device driver until it
 *              finds a driver that will accept name and arg, then call
 *              DeviceOpen for that device.
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
 *              heartbeat_on  Incicates if the heartbeat is configured to be 
 *                      used or not, true if it is, false otherwise
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_known,
 *              adp_device_open_failed
 *              adp_device_already_open
 */
AdpErrs Adp_OpenDevice(const char *name, const char *arg,
                       unsigned int heartbeat_on);

/*
 *  Function: Adp_CloseDevice
 *   Purpose: Close the device used for channels communication.
 *
 *    Params: None
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open
 */
AdpErrs Adp_CloseDevice(void);

/*
 *  Function: Adp_Ioctl
 *   Purpose: Perform miscellaneous control operations on
 *              the device used for channels communication.
 *              This is a minimal veneer to DevSW_Ioctl.
 *
 *    Params:
 *       Input: opcode  Reason code indicating the operation to perform.
 *      In/Out: args    Pointer to opcode-sensitive arguments/result space.
 *
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open, adp_failed
 */
AdpErrs Adp_Ioctl(int opcode, void *args);

/*
 *  Function: Adp_ChannelRegisterRead
 *   Purpose: Register a callback function for received packets on a given
 *              channel
 *
 *    Params:
 *       Input: chan    The channel the callback function is for.
 *
 *              cbfunc  The callback function.  If NULL, then the current
 *                      callback is removed.
 *
 *              cbstate State pointer to pass into the callback function
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open
 *              adp_bad_channel_id
 *
 * Post-conditions: The callback function is responsible for freeing the
 *                      packet that is passed to it, when that packet is
 *                      no longer needed.
 */
#ifdef __cplusplus
    extern "C" {
#endif

    
extern AdpErrs Adp_ChannelRegisterRead(const ChannelID chan,
                               const ChannelCallback cbfunc,
                                void *cbstate);

#ifdef __cplusplus
    }
#endif
/*
 *  Function: Adp_ChannelRead
 *   Purpose: Wait until a packet has been read for a given channel, and
 *              then return it.  Callbacks for other channels are still
 *              active while this read is blocking.
 *
 *  Pre-conditions: No callback has been already been registered for
 *                      the channel.
 *
 *    Params:
 *       Input: chan    The channel to read.
 *
 *      Output: packet  The received packet.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open
 *              adp_bad_channel_id
 *              adp_callback_already_registered
 *
 * Post-conditions: The calling function is responsible for freeing the
 *                      received packet, when that packet is no longer
 *                      needed.
 */
AdpErrs Adp_ChannelRead(const ChannelID chan, Packet **packet);

/*
 *  Function: Adp_ChannelWrite
 *   Purpose: Write a packet to the given channel
 *
 *  Pre-conditions: Channel must have been previously opened.
 *
 *    Params:
 *       Input: chan    The channel to write.
 *
 *              packet  The packet to write.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open
 *              adp_bad_channel_id
 *
 * Post-conditions: The packet being written becomes the "property" of
 *                      Adp_ChannelWrite, which is responsible for freeing
 *                      the packet when it is no longer needed.
 */
AdpErrs Adp_ChannelWrite(const ChannelID chan, Packet *packet);

/*
 *  Function: Adp_ChannelWriteAsync
 *   Purpose: Write a packet to the given channel, but don't wait
 *            for the write to complete before returning.
 *
 *  Pre-conditions: Channel must have been previously opened.
 *
 *    Params:
 *       Input: chan    The channel to write.
 *
 *              packet  The packet to write.
 *
 *   Returns:
 *          OK: adp_ok
 *       Error: adp_device_not_open
 *              adp_bad_channel_id
 *
 * Post-conditions: The packet being written becomes the "property" of
 *                      Adp_ChannelWrite, which is responsible for freeing
 *                      the packet when it is no longer needed.
 */
AdpErrs Adp_ChannelWriteAsync(const ChannelID chan, Packet *packet);

/*
 *  Function: Adp_AsynchronousProcessing
 *   Purpose: This routine should be called from persistent any idle loop
 *              to give the data I/O routines a chance to poll for packet
 *              activity.  Depending upon the requested mode, this routine
 *              may, or may not, block.
 *
 *    Params:
 *       Input: mode    Specifies whether to block until a complete packet
 *                      has been read, all pending writes have completed,
 *                      or not to block at all.
 *
 *   Returns: Nothing.
 */
void Adp_AsynchronousProcessing(const AsyncMode mode);

/*
 * prototype for DC_APPL packet handler
 */
typedef void (*DC_Appl_Handler)(const DeviceDescr *device, Packet *packet);

/*
 * install a handler for DC_APPL packets (can be NULL), returning old one.
 */
DC_Appl_Handler Adp_Install_DC_Appl_Handler(const DC_Appl_Handler handler);

/*
 * prototype for asynchronous processing callback
 */
typedef void (*Adp_Async_Callback)(const DeviceDescr *device,
                                   const struct timeval *const time_now);

/*
 * add an asynchronous processing callback to the list
 * TRUE == okay, FALSE == no more async processing slots
 */
bool Adp_Install_Async_Callback( const Adp_Async_Callback callback_proc );

/*
 * delay for a given period (in microseconds)
 */
void Adp_delay(unsigned int period);

#endif /* ndef angsd_hostchan_h */

/* EOF hostchan.h */
