static char _[] = "@(#)fill.c	5.20 93/07/30 16:38:31, Srini, AMD.";
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
 **       This code provides "fill" routines to fill memory and
 **       registers.  Data may be set as words (32 bit), half-words (16
 **       bit), bytes (8 bit), float (32 bit floating point) or double
 **       (64 bit floating point). 
 **
 **       Since registers are 32 bits long, the fill byte and fill half
 **       commands will only be permitted for memory accesses.
 *****************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <memory.h>
#include "main.h"
#include "memspcs.h"
#include "miniint.h"
#include "macros.h"
#include "error.h"


#ifdef MSDOS
#include <stdlib.h>
#else
#include <malloc.h>
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
** The function below is used in filling data.  This function is
** called in the main command loop parser of the monitor.  The
** parameters passed to this function are:
**
** token - This is an array of pointers to strings.  Each string
**         referenced by this array is a "token" of the user's
**         input, translated to lower case.
**
** token_count - This is the number of items in the token array.
**
** This function reduces the tokens to four parameters:
** the start address of the fill, the end address ofthe fill and
** and the data to be filled in this range.  This data
** is one of the "temp_" variables.
**
*/

#define	MAX_FILL_LEN	128

INT32
fill_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int    result;
   INT32  object_size;
   INT32  align_mask;
   INT32  fill_count;
   struct addr_29k_t addr_29k_start;
   struct addr_29k_t addr_29k_end;
   INT32  temp_word;
   INT16  temp_half;
   BYTE   temp_byte;
   float  temp_float;
   double temp_double;

   INT32	retval;
   BYTE		fill_data[MAX_FILL_LEN]; 


   if (token_count < 4) {
      return (EMSYNTAX);
      }

   /*
   ** What is the data format?
   */

   if ((strcmp(token[0], "f") == 0) ||
       (strcmp(token[0], "fw") == 0)) {
      object_size = sizeof(INT32);
      align_mask = 0xfffffffc;
      result = get_word(token[3], &temp_word);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data( fill_data, (BYTE *)&temp_word, sizeof(INT32));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "fh") == 0) {
      object_size = sizeof(INT16);
      align_mask = 0xfffffffe;
      result = get_half(token[3], &temp_half);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data( fill_data, (BYTE *)&temp_half, sizeof(INT16));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "fb") == 0) {
      object_size = sizeof(BYTE);
      align_mask = 0xffffffff;
      result = get_byte(token[3], &temp_byte);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(fill_data, (BYTE *)&temp_byte, sizeof(BYTE));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "ff") == 0) {
      object_size = sizeof(float);
      align_mask = 0xfffffffc;
      result = get_float(token[3], &temp_float);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(fill_data, (BYTE *)&temp_float, sizeof(float));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "fd") == 0) {
      object_size = sizeof(double);
      align_mask = 0xfffffffc;
      result = get_double(token[3], &temp_double);
      if (result != 0)
         return (EMSYNTAX);
      result = set_data(fill_data, (BYTE *)&temp_double, sizeof(double));
      if (result != 0)
         return (EMSYNTAX);
      }
   else
   if (strcmp(token[0], "fs") == 0) { /* fill_data a string */
      object_size = (INT32) strlen ((char *) token[3]);
      if ((int) object_size >= (int) MAX_FILL_LEN)
	return (EMSYNTAX);
      align_mask = 0xfffffffc;
      (void) memset ((char *) fill_data, (int) '\0', sizeof(fill_data));
      (void) strcpy ((char *)&fill_data[0], (char *) token[3]);
      }
   else
      return(EMSYNTAX);

   /*
   ** Get addresses
   */

   result = get_addr_29k(token[1], &addr_29k_start);
   if (result != 0)
      return (EMSYNTAX);
   result = addr_29k_ok(&addr_29k_start);
   if (result != 0)
      return (result);

   result = get_addr_29k(token[2], &addr_29k_end);
   if (result != 0)
      return (EMSYNTAX);
   result = addr_29k_ok(&addr_29k_end);
   if (result != 0)
      return (result);

   /* Memory spaces must be the same */
   if (addr_29k_start.memory_space != addr_29k_end.memory_space)
      return (EMSYNTAX);

   /* No need to align registers */
   if (ISREG(addr_29k_start.memory_space))
      align_mask = 0xffffffff;

   /* Align addresses */
   addr_29k_start.address = (addr_29k_start.address & align_mask);
   addr_29k_end.address = (addr_29k_end.address & align_mask);

   /* End address must be larger than start address */
   if (addr_29k_start.address > addr_29k_end.address)
      return (EMSYNTAX);

   if (ISREG(addr_29k_end.memory_space)) {
      fill_count = ((addr_29k_end.address -
                     addr_29k_start.address + 1) * 4) /
                    object_size;
      }
   else
   if (ISMEM(addr_29k_end.memory_space)) {
      fill_count = (addr_29k_end.address -
                    addr_29k_start.address +
                    object_size) / object_size;
      }
   else
      return (EMSYNTAX);

   /*
   ** We don't set bytes or half words in registers
   */

   if (ISREG(addr_29k_start.memory_space) &&
        (object_size < sizeof(INT32)))
      return (EMSYNTAX);

   if ((retval = Mini_fill (addr_29k_start.memory_space,
			    addr_29k_start.address,
			    fill_count,
			    object_size,
			    fill_data)) <= TIPFAILURE) {
	return(FAILURE);
   } else if (retval == SUCCESS) {
      return(SUCCESS);
   } else {
      warning(retval);
      return(FAILURE);
   };

   }  /* end set_cmd() */



