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
 * msgbuild.h - utilities for assembling and interpreting ADP messages
 */

#ifndef angel_msgbuild_h
#define angel_msgbuild_h
#include <stdarg.h>
#include "channels.h"

/*
 * msgbuild
 * --------
 * We use a "varargs" function to enable a description of how the
 * final message should look to be provided. We use a function rather
 * than in-line macros to keep the size of Angel small.
 *
 * The "buffer" pointer is the starting point from where the data will
 * be written. Note: If a NULL pointer is passed then no data will be
 * written, but the size information will be returned. This allows
 * code to call this routine with a NULL "buffer" pointer to ascertain
 * whether the pointer they are passing contains enough space for the
 * message being constructed.
 *
 * The "format" string should contain sequences of the following
 * tokens:
 *      %w - insert 32bit word value
 *      %p - insert 32bit target pointer value
 *      %h - insert 16bit value
 *      %b - insert 8bit byte value
 *
 * The return parameter is the final byte length of the data written.
 */
unsigned int msgbuild(unsigned char *buffer, const char *format, ...);
unsigned int vmsgbuild(unsigned char *buffer, const char *format,
                       va_list args);

/*---------------------------------------------------------------------------*/

/*
 * msgsend
 * -------
 * As for msgbuild except that it allocates a buffer, formats the data as
 * for msgbuild and transmits the packet. Returns 0 if successful non 0 if ot
 * fails.
 * Not for use on cooked channels e.g. debug channels only.
 */
extern int msgsend(ChannelID chan, const char *format, ...);

/*---------------------------------------------------------------------------*/

/*
 * Unpack_message
 * --------------
 *    This basically does the opposite of msg_build, it takes a message, and
 * a scanf type format string (but much cut down functionality) and returns
 * the arguments in the message.
 */
extern unsigned int unpack_message(unsigned char *buffer, const char *format, ...);

#endif /* ndef angel_msgbuild_h */

/* EOF msgbuild.h */
