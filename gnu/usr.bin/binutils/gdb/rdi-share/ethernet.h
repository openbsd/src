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
 * ethernet.h:  Angel drivers for Ethernet using Fusion UDP/IP stack
 */
#ifndef angel_ethernet_h
#define angel_ethernet_h

/*
 * the UDP ports that Angel Ethernet uses
 */
#define CTRL_PORT       1913

/*
 * the size of the largest packet accepted on the control socket
 */
#define CTRL_MAXPACKET  6

/*
 * This is the "magic number" sent to the control port to
 * request that the channel port numbers are returned
 */
#define CTRL_MAGIC      "Angel"

/*
 * Array used for responding to a request on the control port
 */
typedef unsigned char CtrlResponse[10];
#define RESP_MAGIC 0
#define RESP_DBUG  6
#define RESP_APPL  8

/*
 * indices for accessing the array of port numbers sent
 * over the control socket
 */
#define DBUG_INDEX      0
#define APPL_INDEX      1

#ifdef TARGET

# include "devdriv.h"

extern const struct angel_DeviceEntry angel_EthernetDevice;

/*
 *  Function: angel_EthernetPoll
 *   Purpose: Poll Fusion for newly arrived packets
 *
 *  Pre-conditions: Called in SVC mode with the lock
 *
 *    Params:
 *       Input: data    IGNORE'd
 *
 *   Returns: Nothing
 *
 * Post-conditions: Will have passed any packets received along to
 *                      higher levels
 */
void angel_EthernetPoll(unsigned int data);

void angel_EthernetNOP(unsigned int data);


/*
 *  Function: angel_FindEthernetConfigBlock
 *   Purpose: Search the Flash for an ethernet config block and return
 *            it if found.
 *
 *    Params: None
 *
 *   Returns: NULL if no config block found, the address if one is found.
 *
 */
extern angel_EthernetConfigBlock *angel_FindEthernetConfigBlock(void);

#else /* def TARGET */

# ifndef COMPILING_ON_WINDOWS
#  define ioctlsocket(x, y, z)  ioctl((x), (y), (z))
#  define closesocket(x)        close(x)
# endif

#endif /* def TARGET */

#endif /* ndef angel_ethernet_h */

/* EOF ethernet.h */
