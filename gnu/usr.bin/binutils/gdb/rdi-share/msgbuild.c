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
 * msgbuild.c - utilities for assembling and interpreting ADP messages
 *
 */

#include <stdarg.h>     /* ANSI varargs support */

#ifdef TARGET
# include "angel.h"
# include "devconf.h"
#else
# include "host.h"
# include "hostchan.h"
#endif

#include "channels.h"
#include "buffers.h"
#include "angel_endian.h"     /* Endianness support macros */
#include "msgbuild.h"   /* Header file for this source code */

#ifndef UNUSED
# define UNUSED(x) ((x)=(x))
#endif

#ifndef TARGET

extern unsigned int Armsd_BufferSize;

#endif /* ndef TARGET */


unsigned int vmsgbuild(unsigned char *buffer, const char *format, va_list args)
{
    unsigned int blen = 0;
    int ch;

    /* Step through the format string */
    while ((ch = *format++) != '\0')
    {
        if (ch != '%')
        {
            if (buffer != NULL)
                *buffer++ = (unsigned char)ch;

            blen++;
        }
        else
        {
            switch (ch = *format++)
            {
              case 'w':
              case 'W':
              /* 32bit pointer */
              case 'p':
              case 'P':
              {
                  /* 32bit word / 32bit pointer */
                  unsigned int na = va_arg(args, unsigned int);

                  if (buffer != NULL)
                  {
                      PUT32LE(buffer, na);
                      buffer += sizeof(unsigned int);
                  }

                  blen += sizeof(unsigned int);

                  break;
              }

              case 'h':
              case 'H':
              {
                  /* 16bit value */
                  unsigned int na = va_arg(args, unsigned int);

                  if (buffer != NULL)
                  {
                      PUT16LE(buffer, na);
                      buffer += sizeof(unsigned short);
                  }

                  blen += sizeof(unsigned short);

                  break;
              }

              case 'c':
              case 'C':
              case 'b':
              case 'B':
                  /* 8bit character / 8bit byte */
                  ch = va_arg(args, int);

                  /*
                   * XXX
                   *
                   * fall through to the normal character processing
                   */

              case '%':
              default:
                  /* normal '%' character, or a different normal character */
                  if (buffer != NULL)
                      *buffer++ = (unsigned char)ch;

                  blen++;
                  break;
            }
        }
    }

    return blen;
}

/*
 * msgbuild
 * --------
 * Simple routine to aid in construction of Angel messages. See the
 * "msgbuild.h" header file for a detailed description of the operation
 * of this routine.
 */
unsigned int msgbuild(unsigned char *buffer, const char *format, ...)
{
    va_list args;
    unsigned int blen;

    va_start(args, format);
    blen = vmsgbuild(buffer, format, args);
    va_end(args);

    return blen;
}

#if !defined(JTAG_ADP_SUPPORTED) && !defined(MSG_UTILS_ONLY)
/*
 * This routine allocates a buffer, puts the data supplied as
 * parameters into the buffer and sends the message. It does *NOT*
 * wait for a reply.
 */
extern int msgsend(ChannelID chan, const char *format,...)
{
    unsigned int length;
    p_Buffer buffer;
    va_list args;
# ifndef TARGET
    Packet *packet;

    packet = DevSW_AllocatePacket(Armsd_BufferSize);
    buffer = packet->pk_buffer;
# else
    buffer = angel_ChannelAllocBuffer(Angel_ChanBuffSize);
# endif

    if (buffer != NULL)
    {
        va_start(args, format);

        length = vmsgbuild(BUFFERDATA(buffer), format, args);

# ifdef TARGET
        angel_ChannelSend(CH_DEFAULT_DEV, chan, buffer, length);
# else
        packet->pk_length = length;
        Adp_ChannelWrite(chan, packet);
# endif

        va_end(args);
        return 0;
    }
    else
        return -1;
}

#endif /* ndef JTAG_ADP_SUPPORTED && ndef MSG_UTILS_ONLY */

/*
 * unpack_message
 * --------------
 */
extern unsigned int unpack_message(unsigned char *buffer, const char *format, ...)
{
    va_list args;
    unsigned int blen = 0;
    int ch;
    char *chp = NULL;

    va_start(args, format);

    /* Step through the format string. */
    while ((ch = *format++) != '\0')
    {
        if (ch != '%')
        {
            if (buffer != NULL)
                ch = (unsigned char)*buffer++;

            blen++;
        }
        else
        {
            switch (ch = *format++)
            {
              case 'w':
              case 'W':
              {
                  /* 32bit word. */
                  unsigned int *nap = va_arg(args, unsigned int*);

                  if (buffer != NULL)
                  {
                      *nap = PREAD32(LE, buffer);
                      buffer += sizeof(unsigned int);
                  }

                  blen += sizeof(unsigned int);

                  break;
              }

              case 'h':
              case 'H':
              {
                  /* 16bit value. */
                  unsigned int *nap = va_arg(args, unsigned int*);

                  if (buffer != NULL)
                  {
                      *nap = PREAD16(LE,buffer);
                      buffer += sizeof(unsigned short);
                  }

                  blen += sizeof(unsigned short);

                  break;
              }

              case 'c':
              case 'C':
              case 'b':
              case 'B':
                  /* 8-bit character, or 8-bit byte */
                  chp = va_arg(args, char*);

                  /*
                   * XXX
                   *
                   * fall through to the normal character processing.
                   */

              case '%':
              default:
                  /* normal '%' character, or a different normal character */
                  if (buffer != NULL)
                      *chp = (unsigned char)*buffer++;

                  blen++;

                  break;
            }
        }
    }

    va_end(args);
    return(blen);
}


/* EOF msgbuild.c */
