static char _[] = "@(#)endian.c	5.18 93/07/30 16:40:17, Srini, AMD.";
/******************************************************************************
 * Copyright 1991 Advanced Micro Devices, Inc.
 *
 * This software is the property of Advanced Micro Devices, Inc  (AMD)  which
 * specifically  grants the user the right to modify, use and distribute this
 * software provided this notice is not removed or altered.  All other rights
 * are reserved by AMD.
 *
 * AMD MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS
 * SOFTWARE.  IN NO EVENT SHALL AMD BE LIABLE FOR INCIDENTAL OR CONSEQUENTIAL
 * DAMAGES IN CONNECTION WITH OR ARISING FROM THE FURNISHING, PERFORMANCE, OR
 * USE OF THIS SOFTWARE.
 *
 * So that all may benefit from your experience, please report  any  problems
 * or  suggestions about this software to the 29K Technical Support Center at
 * 800-29-29-AMD (800-292-9263) in the USA, or 0800-89-1131  in  the  UK,  or
 * 0031-11-1129 in Japan, toll free.  The direct dial number is 512-462-4118.
 *
 * Advanced Micro Devices, Inc.
 * 29K Support Products
 * Mail Stop 573
 * 5900 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 *****************************************************************************
 *      Engineer: Srini Subramanian.
 *****************************************************************************
 ** 
 **       This module implements the endian conversion routines used by MONTIP.
 **
 *****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>
#include  "messages.h"

#ifdef MSDOS
#include <string.h>
#else
#include <string.h>
#endif  /* MSDOS */

/* Functions */
void  tip_convert32 PARAMS((BYTE *));
void  tip_convert16 PARAMS((BYTE *));

/*
** This function is used to convert the endian of messages.
** Both host to target and target to host messages can be
** converted using this function.
**
** Note that all monitor messages have a header consisting of
** a 32 bit message number and a 32 bit size.  Following this
** may be one or more 32 bit parameters.  And folowing these
** parameters may be an array of bytes.
**
** This function converts the endian of the header and any
** parameters.  It is not necessary to convert the array of
** bytes.
**
** Note that the use of 32 bit parameters makes this conversion
** routine fairly simple.
*/

void
endian_cvt(msg_buf, direction)
   union  msg_t  *msg_buf;
   int    direction;
   {
   INT32  i;
   BYTE  *byte;
   INT32  code;
   INT32  length;


   /*
   ** If incoming message, convert endian, then get message
   ** type and message length.  If outgoing message, get
   ** message type and message length, then convert endian.
   */

   if ((direction != OUTGOING_MSG) &&
       (direction != INCOMING_MSG))
      return;

   if (direction == OUTGOING_MSG) {
      code = (msg_buf->generic_msg).code;
      length = (msg_buf->generic_msg).length;
      }

   /* Change endian of "code" field */
   tip_convert32((BYTE *) &(msg_buf->generic_msg).code);

   /* Change endian of "length" field */
   tip_convert32((BYTE *) &(msg_buf->generic_msg).length);

   if (direction == INCOMING_MSG) {
      code = (msg_buf->generic_msg).code;
      length = (msg_buf->generic_msg).length;
      }

   /*
   ** Some messages, notably WRITE_REQ, FILL, READ
   ** and TRACE have data following the message
   ** parameters.  Since we don't want to swap bytes
   ** in the data array, we need to get the number of
   ** of bytes taken up by the parameters.  This is
   ** still better than having to find ALL of the
   ** message lengths statically.
   */

   if (code == WRITE_REQ) 
      length = msg_length(WRITE_REQ);
   else
   if (code == FILL)
      length = MSG_LENGTH(struct fill_msg_t);
   else
   if (code == READ_ACK)
      length = MSG_LENGTH(struct read_ack_msg_t);
   else
   if (code == CHANNEL1)
      length = MSG_LENGTH(struct channel1_msg_t);
   else
   if (code == CHANNEL2)
      length = MSG_LENGTH(struct channel2_msg_t);
   else
   if (code == CHANNEL0)
      length = MSG_LENGTH(struct channel0_msg_t);
   else
   if (code == STDIN_NEEDED_ACK)
      length = MSG_LENGTH(struct stdin_needed_ack_msg_t);

   /* Convert message parameters */

   byte = (BYTE *) &(msg_buf->generic_msg).byte;
   for (i=0; i<(length/sizeof(INT32)); i=i+1) {
      tip_convert32(byte);
      byte = byte + sizeof(INT32);
      }

   }   /* end endian_cvt */


/*
** This function is used to swap the bytes in a 32 bit
** word.  This will convert "little endian" (IBM-PC / Intel)
** words to "big endian" (Sun / Motorola) words.
*/


void
tip_convert32(byte)
   BYTE *byte;
   {
   BYTE temp;

   temp = byte[0];  /* Swap bytes 0 and 3 */
   byte[0] = byte[3];
   byte[3] = temp;
   temp = byte[1];  /* Swap bytes 1 and 2 */
   byte[1] = byte[2];
   byte[2] = temp;
   }   /* end tip_convert32() */


/*
** This function is used to swap the bytes in a 16 bit
** word.  This will convert "little endian" (IBM-PC / Intel)
** half words to "big endian" (Sun / Motorola) half words.
*/

void
tip_convert16(byte)
   BYTE *byte;
   {
   BYTE temp;

   temp = byte[0];  /* Swap bytes 0 and 1 */
   byte[0] = byte[1];
   byte[1] = temp;

   }   /* end tip_convert16() */
