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
 *     Title: Public interface to buffer management
 */

#ifndef angel_buffers_h
#define angel_buffers_h

#include "chandefs.h"           /* CHAN_HEADER_SIZE */


/* the handle to a buffer */
typedef unsigned char *p_Buffer;


/*
 * Angel Packets are structured as a fixed size header, followed
 * by the packet data
 */
#ifdef TARGET
# define BUFFERDATA(b)  (b)     /* channels layer takes care of it */
#else
# define BUFFERDATA(b)  (&((b)[CHAN_HEADER_SIZE]))
#endif


/*
 * The buffer management function prototypes are only applicable
 * when compiling target code
 */
#ifdef TARGET

/*
 * Function: Angel_BufferQuerySizes
 *  Purpose: Request infomation on the default and maximum buffer sizes
 *           that can be allocated
 *
 *   Params:
 *             In/Out: default_size, max_size: pointers to place the
 *                     sizes in on return
 */

void Angel_BufferQuerySizes(unsigned int *default_size, 
                            unsigned int *max_size );

/*
 * Function: Angel_RxEnginBuffersLeft
 *  Purpose: return the number of free buffers 
 *
 *   Params:
 *            Returns: number of free buffers
 */
unsigned int Angel_BuffersLeft( void );

/*
 * Function: Angel_BufferAlloc
 *  Purpose: allocate a buffer that is at least req_size bytes long 
 *
 *   Params:
 *              Input: req_size     the required size of the buffer
 *
 *              Returns: pointer to the buffer NULL if unable to 
 *                       fulfil the request
 */
p_Buffer     Angel_BufferAlloc(unsigned int  req_size);

/*
 * Function: Angel_BufferRelease
 *  Purpose: release a buffer back to the free pool
 *
 *   Params:
 *              Input: pointer to the buffer to free
 */
void Angel_BufferRelease(p_Buffer buffer);


/* return values for angel_InitBuffers */
typedef enum buf_init_error{
  INIT_BUF_OK,
  INIT_BUF_FAIL
} buf_init_error;

/*
 * Function: Angel_InitBuffers
 *  Purpose: Initalised and malloc the buffer pool
 *
 *   Params:
 *              Returns: see above
 */

buf_init_error  Angel_InitBuffers(void);

#endif /* def TARGET */

#endif /* ndef angel_buffers_h */

/* EOF buffers.h */
