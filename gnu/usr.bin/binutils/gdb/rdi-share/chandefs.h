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
 *     Title: Enumeration with all supported channels
 */

#ifndef angel_chandefs_h
#define angel_chandefs_h

enum channelIDs {
  CI_PRIVATE = 0,               /* channels protocol control messages */
  CI_HADP,                      /* ADP, host originated */
  CI_TADP,                      /* ADP, target originated */
  CI_HBOOT,                     /* Boot, host originated */
  CI_TBOOT,                     /* Boot, target originated */
  CI_CLIB,                      /* Semihosting C library support */
  CI_HUDBG,                     /* User debug support, host originated */
  CI_TUDBG,                     /* User debug support, target originated */
  CI_HTDCC,                     /* Thumb direct comms channel, host orig. */
  CI_TTDCC,                     /* Thumb direct comms channel, target orig. */
  CI_TLOG,                      /* Target debug/logging */
  CI_NUM_CHANNELS
};

typedef unsigned ChannelID;


/*
 * Size in bytes of the channel header.
 * This is a duplicate of XXX in chanpriv.h, but we don't want everyone
 * to have access to all of chanpriv.h, so we'll double-check in chanpriv.h.
 */
#define CHAN_HEADER_SIZE (4)

#endif /* ndef angel_chandefs_h */

/* EOF chandefs.h */
