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
 *     Title: Public client interface to devices
 */

#ifndef angel_devclnt_h
#define angel_devclnt_h

/*
 * This header exports the public interface to Angel-compliant device
 * drivers.
 *
 * They are intended to be used solely by Angel, not by the User
 * Application.  See devappl.h for the User Application interface to
 * the device drivers.
 */

#include "devices.h"

/* General purpose constants, macros, enums, typedefs */

/*
 * possible channels at device level
 *
 * XXX
 *
 * these are used as array indices, so be specific about their values
 */
typedef enum DevChanID {
  DC_DBUG = 0,                  /* reliable debug packets
                                 * containing SDBG, CLIB,UDBG, etc.) */
  DC_APPL = 1,                  /* application packets */
  DC_NUM_CHANNELS
} DevChanID;

/* Publically-accessible globals */
/* none */

/* Public functions */

/*
 * Function: angel_DeviceWrite
 *  Purpose: The main entry point for asynchronous writes to a device.
 *
 *   Params:
 *              Input: devID     index of the device to write to
 *                     buff      data to write
 *                     length    how much data to write
 *                     callback  callback here when write finished
 *                                or error
 *                     cb_data   data to be passed to callback
 *                     chanID    device channel to use
 *             Output: -
 *             In/Out: -
 *
 *            Returns: DE_OKAY     write request is underway           
 *                     DE_NO_DEV   no such device                      
 *                     DE_BAD_DEV  device does not support angel writes
 *                     DE_BAD_CHAN no such device channel              
 *                     DE_BUSY     device busy with another write      
 *                     DE_INVAL    silly length                        
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Commence asynchronous transmission of a buffer on a device.  The
 * callback will occur when the write completes or if there is an
 * error.
 *
 * This must be called for each packet to be sent.
 */

DevError angel_DeviceWrite(DeviceID devID, p_Buffer buff,
                           unsigned length, DevWrite_CB_Fn callback,
                           void *cb_data, DevChanID chanID);


/*
 * Function: angel_DeviceRegisterRead
 *  Purpose: The main entry point for asynchronous reads from a device.
 *
 *   Params:
 *              Input: devID     index of the device to read from
 *                     callback  callback here when read finished
 *                                or error
 *                     cb_data   data to be passed to callback
 *                     get_buff  callback to be used to acquire buffer
 *                                for incoming packets
 *                     getb_data data to be passed to get_buff
 *                     chanID    device channel to use
 *             Output: -
 *             In/Out: -
 *
 *            Returns: DE_OKAY     read request is underway           
 *                     DE_NO_DEV   no such device                      
 *                     DE_BAD_DEV  device does not support angel reads
 *                     DE_BAD_CHAN no such device channel              
 *                     DE_BUSY     device busy with another read      
 *                     DE_INVAL    silly length                        
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Register asynchronous packet read from a device.  The callback will
 * occur when the read completes or if there is an error.
 *
 * This is persistent: the read remains registered for all incoming
 * packets on the device channel.
 */

DevError angel_DeviceRegisterRead(DeviceID devID,
                                  DevRead_CB_Fn callback, void *cb_data,
                                  DevGetBuff_Fn get_buff, void *getb_data,
                                  DevChanID chanID);


/*
 * Function: angel_DeviceControl
 *  Purpose: Call a control function for a device
 *
 *   Params:
 *              Input: devID     index of the device to control to
 *                     op        operation to perform
 *                     arg       parameter depending on op
 *
 *            Returns: DE_OKAY     control request is underway           
 *                     DE_NO_DEV   no such device                      
 *                     DE_BAD_OP   device does not support operation
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * Have a device perform a control operation.  Extra parameters vary 
 * according to the operation requested.
 */

DevError angel_DeviceControl(DeviceID devID, DeviceControl op, void *arg);


/*
 * Function: angel_ReceiveMode
 *  Purpose: enable or disable reception across all devices
 *
 *   Params:
 *              Input: mode   choose enable or disable
 *
 * Pass the mode parameter to the receive_mode control method of each device
 */

void angel_ReceiveMode(DevRecvMode mode);


/*
 * Function: angel_ResetDevices
 *  Purpose: reset all devices
 *
 *   Params: none
 *
 * Call the reset control method for each device
 */

void angel_ResetDevices(void);


/*
 * Function: angel_InitialiseDevices
 *  Purpose: initialise the device driver layer
 *
 *   Params: none
 *
 * Set up the device driver layer and call the init method for each device
 */

void angel_InitialiseDevices(void);


/*
 * Function: angel_IsAngelDevice
 *  Purpose: Find out if a device supports Angel packets
 *
 *   Params:
 *              Input: devID     index of the device to control to
 *
 *            Returns: TRUE      supports Angel packets
 *                     FALSE     raw device
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 */

bool angel_IsAngelDevice(DeviceID devID);


#if !defined(MINIMAL_ANGEL) || MINIMAL_ANGEL == 0

/*
 * Function: angel_ApplDeviceHandler
 *  Purpose: The entry point for User Application Device Driver requests
 *           in a full functiionality version of Angel.
 *           It will never be called directly by the User Application,
 *           but gets called indirectly, via the SWI handler.
 *
 *  Params:
 *      Input: swi_r0    Argument to SWI indicating that 
 *                       angel_ApplDeviceHandler was to be called.  This
 *                       will not be used in this function, but is needed
 *                       by the SWI handler.
 *             arg_blk   pointer to block of arguments
 *                       arg_blk[0] is one of
 *                       angel_SWIreason_ApplDevice_{Read,Write,Yield}
 *                       which indicates which angel_Device* fn is to
 *                       be called.  arg_blk[1] - arg_blk[n] are the
 *                       arguments to the corresponding
 *                       angel_ApplDevice* function.
 *             Output: -
 *             In/Out: -
 *
 *            Returns:   whatever the specified angel_Device* function
 *                       returns.
 *
 *      Reads globals: -
 *   Modifies globals: -
 *
 * Other side effects: -
 *
 * This has the side effects of angel_Device{Read,Write,Yield}
 * depending upon which is operation is specified as described above.
 */

DevError angel_ApplDeviceHandler(
  unsigned swi_r0, unsigned *arg_blk
);

#endif /* ndef MINIMAL_ANGEL */

#endif /* ndef angel_devclnt_h */

/* EOF devclnt.h */
