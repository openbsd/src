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
 *     Title: Devices header file
 */

#ifndef angel_devices_h
#define angel_devices_h

/*
 * Provides common types for using devices, and provides access to the
 * device table.
 */

#include "angel.h"
#include "buffers.h"

/* General purpose constants, macros, enums, typedefs */

/* a non-enum holder for device IDs */
typedef unsigned int DeviceID;

/* device error codes */
typedef enum DevError {
  DE_OKAY,     /* no error */
  DE_NO_DEV,   /* no such device */
  DE_BAD_DEV,  /* device does not support angel */
  DE_BAD_CHAN, /* no such device channel */
  DE_BAD_OP,   /* operation not supported by this device */
  DE_BUSY,     /* device already busy */
  DE_INVAL,    /* length invalid */
  DE_FAILED    /* something else went wrong */
} DevError;

/* return codes from asynchronous calls - primarily for channels' benefit */
typedef enum DevStatus {
  DS_DONE,                      /* operation succeeded */
  DS_OVERFLOW,                  /* not enough buffer space */
  DS_BAD_PACKET,                /* packet failed */
  DS_DEV_ERROR,                 /* device error */
  DS_INT_ERROR                  /* internal error */
} DevStatus;

/* Callback for async. writes */
typedef void (*DevWrite_CB_Fn)(
    void *buff,     /* pointer to data -- cast to p_Buffer  */
    void *length,   /* how much done   -- cast to unsigned  */
    void *status,   /* success code    -- cast to DevStatus */
    void *cb_data   /* as supplied */
    );

/* Callback for async. reads */
typedef void (*DevRead_CB_Fn)(
    void *buff,     /* pointer to data -- cast to p_Buffer  */
    void *length,   /* how much read   -- cast to unsigned  */
    void *status,   /* success code    -- cast to DevStatus */
    void *cb_data   /* as supplied */
    );

/* control operations */
typedef enum DeviceControl {
  DC_INIT,                      /* initialise device             */
  DC_RESET,                     /* reset device                  */
  DC_RECEIVE_MODE,              /* control reception             */
  DC_SET_PARAMS,                /* set parameters of device      */
#ifndef TARGET
  DC_GET_USER_PARAMS,           /* params set by user at open    */
  DC_GET_DEFAULT_PARAMS,        /* device default parameters     */
  DC_RESYNC,                    /* resynchronise with new agent  */
#endif
  DC_PRIVATE                    /* start of private device codes */
} DeviceControl;

typedef enum DevRecvMode {
  DR_DISABLE,
  DR_ENABLE
} DevRecvMode;

/*
 * callback to allow a device driver to request a buffer, to be filled
 * with an incoming packet
 */
typedef p_Buffer (*DevGetBuff_Fn)(unsigned req_size, void *cb_data);


/* Publically-accessible globals */
/* none */

#endif /* ndef angel_devices_h */

/* EOF devices.h */
