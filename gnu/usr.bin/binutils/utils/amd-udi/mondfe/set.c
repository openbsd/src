static char _[] = "@(#)set.c	5.20 93/07/30 16:39:00, Srini, AMD.";
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
 **       This code provides "set" routines to set memory and
 **       registers.  Data may be set as words (32 bit), half-words
 **       (16 bit), bytes (8 bit), float (32 bit floating point) or
 **       double (64 bit floating point).
 **
 **       Since registers are 32 bits long, the set byte and set half
 **       commands will only be permitted for memory accesses.
 *****************************************************************************
 */


#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include "main.h"
#include "monitor.h"
#include "miniint.h"
#include "memspcs.h"
#include "macros.h"
#include "error.h"


#ifdef MSDOS
#include <stdlib.h>
#include <string.h>
#else
#include <string.h>

#endif

int   get_addr_29k PARAMS((char *, struct addr_29k_t *));
int   addr_29k_ok PARAMS((struct addr_29k_t *));

int   get_word PARAMS((char *, INT32 *));
int   get_half PARAMS((char *, INT16 *));
int   get_byte PARAMS((char *, BYTE *));
int   get_float PARAMS((char *, float *));
int   get_double PARAMS((char *, double *));

int   set_data PARAMS((BYTE *, BYTE *, int));

/*
** The function below is used in setting data.  This function is
** called in the main command loop parser of the monitor.  The
** parameters passed to this function are:
**
** token - This is an array of pointers to strings.  Each string
**         referenced by this array is a "token" of the user's
**         input, translated to lower case.
**
** token_count - This is the number of items in the token array.
**
** This function reduces the tokens to three parameters:
** memory_space, address and the data to be set.  This data
** is one of the "temp_" variables.
**
*/


INT32
set_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   INT16  size;
   int    result;
   struct addr_29k_t addr_29k;
   int    set_format;
   INT32  temp_word;
   INT16  temp_half;
   BYTE   temp_byte;
   float  temp_float;
   double temp_double;

   INT32	retval;
   BYTE		write_buffer[16];  /* */
   INT32	bytes_ret;
   INT32	hostendian;     /* UDI conformant */
   INT32	count;


   if (token_count != 3) {
      return (EMSYNTAX);
      }

   /*
   ** What is the data format?
   */

   count = (INT32) 1;
   if ((strcmp(token[0], "s") == 0) ||
       (strcmp(token[0], "sw") == 0)) {
      set_format = WORD_FORMAT;
      size = (INT16) sizeof(INT32);
      result = get_word(token[2], &temp_word);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(write_buffer, (BYTE *)&temp_word, sizeof(INT32));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "sh") == 0) {
      set_format = HALF_FORMAT;
      size = (INT16) sizeof(INT16);
      result = get_half(token[2], &temp_half);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(write_buffer, (BYTE *)&temp_half, sizeof(INT16));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "sb") == 0) {
      set_format = BYTE_FORMAT;
      size = (INT16) sizeof(BYTE);
      result = get_byte(token[2], &temp_byte);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(write_buffer, (BYTE *)&temp_byte, sizeof(BYTE));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "sf") == 0) {
      set_format = FLOAT_FORMAT;
      size = (INT16) sizeof(float);
      result = get_float(token[2], &temp_float);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(write_buffer, (BYTE *)&temp_float, sizeof(float));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "sd") == 0) {
      set_format = DOUBLE_FORMAT;
      size = (INT16) sizeof(double);
      result = get_double(token[2], &temp_double);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(write_buffer, (BYTE *)&temp_double, sizeof(double));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
      return(EMSYNTAX);

   /*
   ** Get address
   */

   result = get_addr_29k(token[1], &addr_29k);
   if (result != 0)
      return (EMSYNTAX);
   result = addr_29k_ok(&addr_29k);
   if (result != 0)
      return (result);

   /*
   ** We don't set bytes or half words in registers
   */

   if (((ISREG(addr_29k.memory_space)) &&
        (set_format == BYTE_FORMAT)) ||

      ((ISREG(addr_29k.memory_space)) &&
        (set_format == HALF_FORMAT)))
      return (EMSYNTAX);

   /* Will the data overflow the message buffer? Done in TIP */

   hostendian = FALSE;
   if ((retval = Mini_write_req (addr_29k.memory_space,
				 addr_29k.address,
				 count,
				 (INT16) size,
				 &bytes_ret,
				 write_buffer,
				 hostendian)) != SUCCESS) {
	return(FAILURE);
   } else 
      return(SUCCESS);

   }  /* end set_cmd() */


