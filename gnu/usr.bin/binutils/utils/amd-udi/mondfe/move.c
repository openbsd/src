static char _[] = "@(#)move.c	5.20 93/07/30 16:38:55, Srini, AMD.";
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
 **       This code provides "move" routines to copy blocks of memory.
 **       Data may be moved as words (32 bit), half-words (16 bit),
 **       bytes (8 bit), float (32 bit floating point) or double
 **       (64 bit floating point). 
 **
 **       Registers moves are not permitted.
 *****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include "main.h"
#include "memspcs.h"
#include "miniint.h"
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

int   set_data PARAMS((BYTE *, BYTE *, int));

/*
** The function below is used in moving data.  This function is
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
** the start address and the end address of the move and the
** target address of the move.
**
*/


INT32
move_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int    result;
   INT32  byte_count;
   struct addr_29k_t addr_29k_start;
   struct addr_29k_t addr_29k_end;
   struct addr_29k_t addr_29k_dest;
   struct addr_29k_t temp_addr_29k;

   INT32	retval;
   INT32	direction;

   if ((strcmp(token[0], "m") != 0) ||
       (token_count != 4))
      return (EMSYNTAX);

   /*
   ** Get addresses
   */

   result = get_addr_29k(token[1], &addr_29k_start);
   if (result != 0)
      return (result);
   result = addr_29k_ok(&addr_29k_start);
   if (result != 0)
      return (result);

   result = get_addr_29k(token[2], &addr_29k_end);
   if (result != 0)
      return (result);
   result = addr_29k_ok(&addr_29k_end);
   if (result != 0)
      return (result);

   result = get_addr_29k(token[3], &addr_29k_dest);
   if (result != 0)
      return (result);
   result = addr_29k_ok(&addr_29k_dest);
   if (result != 0)
      return (result);

   /* End address must be not be less than start address */
   if (addr_29k_start.address > addr_29k_end.address)
      return (EMSYNTAX);

   byte_count = (addr_29k_end.address - addr_29k_start.address) + 1;

   /* Dest range must be in valid memory */
   temp_addr_29k.memory_space = addr_29k_dest.memory_space;
   /* For memory to register, divide byte count by 4 */
   if ((ISMEM(addr_29k_start.memory_space)) &&
       (ISREG(addr_29k_dest.memory_space)))
      temp_addr_29k.address = addr_29k_dest.address + (byte_count / 4);
   else
   /* For register to memory, multiply byte count by 4 */
   if ((ISREG(addr_29k_start.memory_space)) &&
       (ISMEM(addr_29k_dest.memory_space)))
      temp_addr_29k.address = addr_29k_dest.address + (byte_count * 4);
   else
      temp_addr_29k.address = addr_29k_dest.address + byte_count;
   result = addr_29k_ok(&temp_addr_29k);
   if (result != 0)
      return (EMCOPY);

   /* Registers are four bytes */
   if (ISREG(addr_29k_start.memory_space))
      byte_count = (byte_count * 4);

   /* Do copy */

   direction = TRUE; /* SRINI front to back or back to front */
   if ((retval = Mini_copy (addr_29k_start.memory_space,
			    addr_29k_start.address,
			    addr_29k_dest.memory_space,
			    addr_29k_dest.address,
			    byte_count,
			    (INT16) 1, /* size */
			    direction)) != SUCCESS) {
	return(FAILURE);
   } else 
      return(SUCCESS);

   }  /* end move_cmd() */



