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
 *     Title: User interface to the channels layer
 */

#ifndef angel_channels_h
#define angel_channels_h

/*
 * This provides the public interface to the channels layer read and write
 * routines, and buffer management routines.
 */

/* Nested header files, if required */

#include "devices.h"
#include "chandefs.h"
#include "adperr.h"

/* General purpose constants, macros, enums, typedefs */

/* use the default device */
#define CH_DEFAULT_DEV ((DeviceID)-1)

/* return codes */
typedef enum ChanError {
  CE_OKAY,                      /* no error */
  CE_ABANDONED,                 /* abandoned due to device switch */
  CE_DEV_ERROR,                 /* unexpected error from device driver */
  CE_BUSY,                      /* channel in use */
  CE_BUFF_ERROR,                /* unable to get buffer */
  CE_PRIVATE                    /* start of internal error codes */
} ChanError;


/* Publically-accessible globals */

/*
 * The following two globals are only valid after angel_InitialiseChannels()
 * has been called.
 */

/* the default size of a channel buffer, for global use */
extern unsigned Angel_ChanBuffSize;

/* the size of a long buffer, for global use */
extern unsigned Angel_ChanLongSize;

#ifdef TARGET
AdpErrs send_resend_msg(DeviceID devid);
#endif

/*
 * Function: angel_InitialiseChannels
 *  Purpose: initialise the channels layer
 *
 *   Params:
 *              Input: -
 *             Output: -
 *             In/Out: -
 *
 *            Returns: -
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 */

void angel_InitialiseChannels( void );

/*
 * Function: adp_init_seq
 *  Purpose: initialise sequence numbers and free anyt leftover buffers
 *
 *   Params:
 *              Input: -
 *             Output: -
 *             In/Out: -
 *
 *            Returns: - adp_ok if things went ok else an error code
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 */

AdpErrs adp_init_seq(void);

/*
 * Function: angel_ChannelAllocBuffer
 *  Purpose: allocate a buffer that is at least req_size bytes long
 *
 *   Params:
 *              Input: req_size        the minimum size required
 *             Output: -
 *             In/Out: -
 *
 *            Returns: pointer to allocated buffer, or
 *                     NULL if unable to allocate suitable buffer
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 */

p_Buffer angel_ChannelAllocBuffer(unsigned req_size);


/*
 * Function: angel_ChannelReleaseBuffer
 *  Purpose: release a buffer back to the free pool
 *
 *   Params:
 *              Input: buffer   the buffer to release
 *             Output: -
 *             In/Out: -
 *
 *            Returns: -
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 */

void angel_ChannelReleaseBuffer(p_Buffer buffer);


/*
 * Function: angel_ChannelSend
 *  Purpose: blocking send of a packet via a channel
 *
 *   Params:
 *              Input: devid         Device to use, or CH_DEFAULT_DEV
 *                     chanid        Channel to use for tx 
 *                     buffer        Pointer to data to send
 *                     len           Length of data to send
 *             Output: -
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Transmission completed
 *                     CE_BAD_CHAN   Channel id invalid
 *                     CE_ABANDONED  Tx abandoned due to device switch
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 */

ChanError angel_ChannelSend(DeviceID devid, ChannelID chanid,
                            const p_Buffer buffer, unsigned len);


/*
 * Function: angel_ChannelSendAsync
 *  Purpose: asynchronous send of a packet via a channel
 *
 *   Params:
 *              Input: devid         Device to use, or CH_DEFAULT_DEV
 *                     chanid        Channel to use for tx 
 *                     buffer        Pointer to data to send
 *                     len           Length of data to send
 *                     callback      Function to call on completion
 *                     callback_data Pointer to pass to callback
 *             Output: -
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Transmission underway
 *                     CE_BAD_CHAN   Channel id invalid
 *                     CE_ABANDONED  Tx abandoned due to device switch
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * register an asynchronous send on the given channel
 * (blocks until send can be commenced)
 */

typedef void (*ChanTx_CB_Fn)(ChannelID  chanid,         /* which channel  */
                             void      *callback_data); /* as supplied... */
                             

ChanError angel_ChannelSendAsync(          DeviceID      devid,
                                           ChannelID     chanid,
                                     const p_Buffer      buffer,
                                           unsigned      len, 
                                           ChanTx_CB_Fn  callback, 
                                           void         *callback_data);


/*
 * Function: angel_ChannelRead
 *  Purpose: blocking read of a packet from a channel
 *
 *   Params:
 *              Input: devid         Device to use, or CH_DEFAULT_DEV
 *                     chanid        Channel to use for rx
 *             Output: buffer        The buffer, supplied and filled
 *                     len           How many bytes there are in the buffer
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Reception successful
 *                     CE_BAD_CHAN   Channel id invalid
 *                     CE_ABANDONED  Tx abandoned due to device switch
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Note that in the present version, if an asynchronous read has been
 * registered, a blocking read will be refused with CE_BUSY.
 */
ChanError angel_ChannelRead(DeviceID      devid,
                            ChannelID     chanid,
                            p_Buffer     *buffer, 
                            unsigned     *len);


/*
 * Function: angel_ChannelReadAsync
 *  Purpose: asynchronous read of a packet via a channel
 *
 *   Params:
 *              Input: devid         Device to use, or CH_DEFAULT_DEV
 *                     chanid        Channel to wait on
 *                     callback      Function to call on completion, or NULL
 *                     callback_data Pointer to pass to callback
 *             Output: -
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Read request registered
 *                     CE_BAD_CHAN   Channel id invalid
 *                     CE_BUSY       Someone else is using the channel
 *                                   (in a single threaded world)
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Register an asynchronous read on the given channel.  There can only be one
 * async. reader per channel, and blocking reads are not permitted whilst
 * an async. reader is registered.
 *
 * Reader can unregister by specifying NULL as the callback function.
 */

typedef void (*ChanRx_CB_Fn)(DeviceID   devID,   /* ID of receiving device  */
                             ChannelID  chanID,  /* ID of receiving channel */
                             p_Buffer   buff,    /* pointer to buffer       */
                             unsigned   len,     /* length of data          */
                             void      *cb_data  /* callback data           */
                             );

ChanError angel_ChannelReadAsync(DeviceID      devid,
                                 ChannelID     chanid,
                                 ChanRx_CB_Fn  callback, 
                                 void         *callback_data);


/*
 * Function: angel_ChannelReadAll
 *  Purpose: register an asynchronous read across all devices
 *
 *   Params:
 *              Input: chanid        Channel to look for (usually HBOOT)
 *                     callback      Function to call on completion
 *                     callback_data Pointer to pass to callback
 *             Output: -
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Read request registered
 *                     CE_BAD_CHAN   Channel id invalid
 *                     CE_BUSY       Someone else is reading all devices
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Register an asynchronous read across all devices.  This is a 'fallback',
 * which will be superseded (temporarily) by a registered reader or blocking 
 * read on a specific device.
 */

ChanError angel_ChannelReadAll(         ChannelID     chanid,
                                        ChanRx_CB_Fn  callback,
                                        void         *callback_data);



/*
 * Function: angel_ChannelSendThenRead
 *  Purpose: blocking write to followed by read from a channel
 *
 *   Params:
 *              Input: devid         Device to use, or CH_DEFAULT_DEV
 *                     chanid        Channel to use for rx
 *             In/Out: buffer        On entry:  the packet to be sent
 *                                   On return: the packet received
 *                     len           On entry:  length of packet to be sent
 *                                   On return: length of packet rx'd
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Tx and Reception successful
 *                     CE_BAD_CHAN   Channel id invalid
 *                     CE_ABANDONED  Tx abandoned due to device switch
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Note that in the present version, if an asynchronous read has been
 * registered, this will be refused with CE_BUSY.
 */
ChanError angel_ChannelSendThenRead(DeviceID      devid,
                                    ChannelID     chanid,
                                    p_Buffer     *buffer, 
                                    unsigned     *len);


/*
 * Function: angel_ChannelSelectDevice
 *  Purpose: select the device to be used for all channel comms
 *
 *   Params:
 *              Input: device        ID of device to use as the default
 *             Output: -
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Default device selected
 *                     CE_BAD_DEV    Invalid device ID
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: Any channel operations in progress are
 *                     abandoned.
 *
 * select the device for all channels comms
 */

ChanError angel_ChannelSelectDevice(DeviceID device);


/*
 * Function: angel_ChannelReadActiveDevice
 *  Purpose: reads the device id of the currently active device
 *
 *   Params:
 *              Input: device        address of a DeviceID variable
 *             Output: *device       ID of device currently being used
 *             In/Out: -
 *
 *            Returns: CE_OKAY       Default device selected
 */

ChanError angel_ChannelReadActiveDevice(DeviceID *device);

#endif /* ndef angel_channels_h */

/* EOF channels.h */
