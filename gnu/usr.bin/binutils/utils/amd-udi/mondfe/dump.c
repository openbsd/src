static char _[] = " @(#)dump.c	5.20 93/07/30 16:38:27, Srini, AMD ";
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
 **       This code provides dump routines to output data in
 **       hex / ASCII formats. 
 **
 *****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include "main.h"
#include "macros.h"
#include "monitor.h"
#include "miniint.h"
#include "memspcs.h"
#include "error.h"


#ifdef MSDOS
#include <stdlib.h>
#include <string.h>
#else
#include <string.h>
#endif

int   get_addr_29k PARAMS((char *, struct addr_29k_t *));
int   addr_29k_ok PARAMS((struct addr_29k_t *));
int   print_addr_29k PARAMS((INT32, ADDR32));

int   dump_mem_word PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_reg_word PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_mem_half PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_reg_half PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_mem_byte PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_reg_byte PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_mem_float PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_reg_float PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_mem_double PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));
int   dump_reg_double PARAMS((INT32 mspace, ADDR32 addr, INT32 bytes, BYTE *buf));

int   get_data PARAMS((BYTE *, BYTE *, int));
int   dump_ASCII PARAMS((char *, int, BYTE *, int));


/*
** The function below is used in dumping data.  This function is
** called in the main command loop parser of the monitor.  The
** parameters passed to this function are:
**
** token - This is an array of pointers to strings.  Each string
**         referenced by this array is a "token" of the user's
**         input, translated to lower case.
**
** token_count - This is the number of tokens in "token".
**
** This function reduces the tokens to three parameters:
** memory_space, address and byte_count.  The address parameter is
** aligned as follows:
**
**    - All register accesses are byte aligned.  The address,
**      however, accesses 32 bit words.  The valued in these
**      registers are displayed in formats determined by the
**      first token.
**
**    - Memory addresses are aligned and displayed according
**      to the dump format as specified in the first token.
** 
**
*/


INT32
dump_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   static INT32  memory_space=D_MEM;
   static ADDR32 address=0;
   INT32  byte_count=64;
   int    result;
   struct addr_29k_t addr_29k_start;
   struct addr_29k_t addr_29k_end;
   int    dump_format;
   int    object_size;
   ADDR32 align_mask;

   INT32	retval;
   INT32	hostendian;
   INT32	bytes_returned;
   BYTE		*read_buffer;

   /*
   ** What is the dump format?
   */

   if ((strcmp(token[0], "d") == 0) ||
       (strcmp(token[0], "dw") == 0)) {
      dump_format = WORD_FORMAT;
      object_size = sizeof(INT32);
      align_mask = 0xfffffffc;
      }
   else
   if (strcmp(token[0], "dh") == 0) {
      dump_format = HALF_FORMAT;
      object_size = sizeof(INT16);
      align_mask = 0xfffffffe;
      }
   else
   if (strcmp(token[0], "db") == 0) {
      dump_format = BYTE_FORMAT;
      object_size = sizeof(BYTE);
      align_mask = 0xffffffff;
      }
   else
   if (strcmp(token[0], "df") == 0) {
      dump_format = FLOAT_FORMAT;
      object_size = sizeof(float);
      align_mask = 0xfffffffc;
      }
   else
   if (strcmp(token[0], "dd") == 0) {
      dump_format = DOUBLE_FORMAT;
      object_size = sizeof(double);
      align_mask = 0xfffffff8;
      }
   else
      return(EMSYNTAX);

   /*
   ** Get start address and byte count
   */

   if (token_count == 1) {
      if (ISREG(memory_space))
         address = address + (byte_count/4);
      else
      if (ISMEM(memory_space))
         address = address + byte_count;
      else
         return(EMBADADDR);
      /* Check the start address */
      addr_29k_start.address = address;
      addr_29k_start.memory_space = memory_space;
      result = addr_29k_ok(&addr_29k_start);
      if (result != 0)
         return (result);
      }
   else
   if (token_count == 2) {
      result = get_addr_29k(token[1], &addr_29k_start);
      if (result != 0)
         return (EMSYNTAX);
      /* Check the start address */
      result = addr_29k_ok(&addr_29k_start);
      if (result != 0)
         return (result);

      memory_space = addr_29k_start.memory_space;
      /* Make sure we have an even multiple of object_size */
      if (ISREG(memory_space)) {
         address = addr_29k_start.address;
         byte_count = (byte_count + (object_size - 1)) & 0xfffffffc;
         }
      else
      if (ISMEM(memory_space)) {
         address = addr_29k_start.address & align_mask;
         byte_count = (byte_count + (object_size - 1)) & align_mask;
         }
      else
         return(EMBADADDR);
      }
   else
   if (token_count == 3) {
      result = get_addr_29k(token[1], &addr_29k_start);
      if (result != 0)
         return (EMSYNTAX);
      /* Only check the start address */
      result = addr_29k_ok(&addr_29k_start);
      if (result != 0)
         return (result);
      result = get_addr_29k(token[2], &addr_29k_end);
      if (result != 0)
         return (EMSYNTAX);

      if (addr_29k_start.memory_space != addr_29k_end.memory_space)
         return (EMBADADDR);
      if (addr_29k_start.address > addr_29k_end.address)
         return (EMBADADDR);

      memory_space = addr_29k_start.memory_space;
      if (ISREG(memory_space)) {
         address = addr_29k_start.address;
         byte_count = (addr_29k_end.address -
                       addr_29k_start.address + 1) * 4;
         }
      else
      if (ISMEM(memory_space)) {
         address = addr_29k_start.address & align_mask;
         byte_count = ((addr_29k_end.address & align_mask) -
                       (addr_29k_start.address & align_mask) +
                      object_size);
         }
      else
         return(EMBADADDR);

      }
   else
   /* Too many args */
      return (EMSYNTAX);


   /*
   ** Get data
   */

   /* Will the data overflow the message buffer? Done by TIP ??*/
   if ((read_buffer = (BYTE *) malloc((unsigned int) byte_count)) == NULL) {
       warning(EMALLOC);
       return(FAILURE);
   };

   hostendian = FALSE;
   if ((retval = Mini_read_req(memory_space,
				address,
				byte_count / object_size,
				(INT16) object_size, 
				&bytes_returned,
				read_buffer,
				hostendian)) != SUCCESS) {
	return(FAILURE);
   };
  
   bytes_returned = bytes_returned * object_size;
    
    /* Continue if SUCCESSful */
    
   /*
   ** Call data format routines
   */

   if ISMEM(memory_space) {
      if (dump_format == WORD_FORMAT)
         result = dump_mem_word(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == HALF_FORMAT)
         result = dump_mem_half(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == BYTE_FORMAT)
         result = dump_mem_byte(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == FLOAT_FORMAT)
         result = dump_mem_float(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == DOUBLE_FORMAT)
         result = dump_mem_double(memory_space,
				address,
				bytes_returned,
				read_buffer);
      }
   else
   if ISREG(memory_space) {
      if (dump_format == WORD_FORMAT)
         result = dump_reg_word(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == HALF_FORMAT)
         result = dump_reg_half(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == BYTE_FORMAT)
         result = dump_reg_byte(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == FLOAT_FORMAT)
         result = dump_reg_float(memory_space,
				address,
				bytes_returned,
				read_buffer);
      else
      if (dump_format == DOUBLE_FORMAT)
         result = dump_reg_double(memory_space,
				address,
				bytes_returned,
				read_buffer);
      }
   else
      return(EMBADADDR);

   (void) free ((char *) read_buffer);
   return (result);

   }  /* end dump_cmd() */



/*
** Functions used by dump_cmd()
*/


/*
** This function is used to dump 32 bit words of data.
** the address is printed, followed by the data, grouped
** into 8 character long strings, each representing one
** 32 bit word.  Space for four 32-bit words is reserved
** on each line.  Following the hex data, an ASCII
** representation of the data is printed.
*/

int
dump_mem_word(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   INT32    data_word;
   struct   addr_29k_t addr_29k;
   int      ASCII_index;
   char     ASCII_buffer[20];

   byte_count = 0;
   ASCII_index = 0;
   ASCII_buffer[0] = '\0';

   address_mask = 0xfffffff0;
   start_address = read_address;
   end_address = read_address + bytes_returned;
   last_print_address = (end_address + 0xf) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         fprintf(stderr, "\n\n");
         return (0);
         }

      /* Print out ASCII data */
      if ((address & address_mask) == address) {
         fprintf(stderr, "  %s\n", ASCII_buffer);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "  %s\n", ASCII_buffer);
         ASCII_index = 0;
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address  != last_print_address)) {
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "         ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "         ");
         result = dump_ASCII(ASCII_buffer, ASCII_index,
                             (BYTE *) NULL, sizeof(INT32)); 
         ASCII_index = ASCII_index + sizeof(INT32);
         address = address + sizeof(INT32);
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_word,
                           &read_buffer[byte_count],
                           sizeof(INT32));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%08lx ", data_word);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%08lx ", data_word);

         /* Build ASCII srting */
         result = dump_ASCII(ASCII_buffer,
                             ASCII_index,
                             &read_buffer[byte_count],
                             sizeof(INT32)); 
         ASCII_index = ASCII_index + sizeof(INT32);

         address = address + sizeof(INT32);

         byte_count = byte_count + sizeof(INT32);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);

   }  /* end dump_mem_word() */


int
dump_reg_word(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   INT32    data_word;
   struct   addr_29k_t addr_29k;
   int      ASCII_index;
   char     ASCII_buffer[20];

   byte_count = 0;
   ASCII_index = 0;
   ASCII_buffer[0] = '\0';

   address_mask = 0xfffffffc;
   start_address = read_address;
   end_address = read_address + (bytes_returned / 4);
   last_print_address = (end_address + 0x3) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print out ASCII data */
      if ((address & address_mask) == address) {
         fprintf(stderr, "  %s\n", ASCII_buffer);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "  %s\n", ASCII_buffer);
         ASCII_index = 0;
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address  != last_print_address)) {
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "         ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "         ");
         result = dump_ASCII(ASCII_buffer, ASCII_index,
                             (BYTE *) NULL, sizeof(INT32)); 
         ASCII_index = ASCII_index + sizeof(INT32);
         address = address + 1;
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_word,
                           &read_buffer[byte_count],
                           sizeof(INT32));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%08lx ", data_word);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%08lx ", data_word);

         /* Build ASCII srting */
         result = dump_ASCII(ASCII_buffer,
                             ASCII_index,
                             &read_buffer[byte_count],
                             sizeof(INT32)); 
         ASCII_index = ASCII_index + sizeof(INT32);

         address = address + 1;

         byte_count = byte_count + sizeof(INT32);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);

   }  /* end dump_reg_word() */



/*
** This function is used to dump memory as half words.
*/

int
dump_mem_half(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   INT16    data_half;
   INT32    data_word;
   struct   addr_29k_t addr_29k;
   int      ASCII_index;
   char     ASCII_buffer[20];

   byte_count = 0;
   ASCII_index = 0;
   ASCII_buffer[0] = '\0';

   address_mask = 0xfffffff0;
   start_address = read_address;
   end_address = read_address + bytes_returned;
   last_print_address = (end_address + 0xf) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print out ASCII data */
      if ((address & address_mask) == address) {
         fprintf(stderr, "  %s\n", ASCII_buffer);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "  %s\n", ASCII_buffer);
         ASCII_index = 0;
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "     ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "     ");
         result = dump_ASCII(ASCII_buffer, ASCII_index,
                             (BYTE *) NULL, sizeof(INT16));
         ASCII_index = ASCII_index + sizeof(INT16);
         address = address + sizeof(INT16);
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_half,
                           &read_buffer[byte_count],
                           sizeof(INT16));
         if (result != 0)
            return (EMBADADDR);

         /* We have to cast to INT32 to print out a hex halfword */
         /* (the Sun libraries sign extend to 32 bits) */
         data_word = (INT32) data_half;
         data_word = (data_word & 0x0000ffff);
         fprintf(stderr, "%04x ", data_word);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%04x ", data_word);

         /* Build ASCII srting */
         result = dump_ASCII(ASCII_buffer,
                             ASCII_index,
                             &read_buffer[byte_count],
                             sizeof(INT16)); 
         ASCII_index = ASCII_index + sizeof(INT16);

         address = address + sizeof(INT16);

         byte_count = byte_count + sizeof(INT16);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_mem_half() */



/*
** This function is used to dump registers as half words.
*/

int
dump_reg_half(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   INT32    data_word;
   struct   addr_29k_t addr_29k;
   int      ASCII_index;
   char     ASCII_buffer[20];

   byte_count = 0;
   ASCII_index = 0;
   ASCII_buffer[0] = '\0';

   address_mask = 0xfffffffc;
   start_address = read_address;
   end_address = read_address + (bytes_returned / 4);
   last_print_address = (end_address + 0x3) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print out ASCII data */
      if ((address & address_mask) == address) {
         fprintf(stderr, "  %s\n", ASCII_buffer);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "  %s\n", ASCII_buffer);
         ASCII_index = 0;
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "         ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "         ");
         result = dump_ASCII(ASCII_buffer, ASCII_index,
                             (BYTE *) NULL, sizeof(INT16));
         ASCII_index = ASCII_index + sizeof(INT16);

         address = address + 1;
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_word,
                           &read_buffer[byte_count],
                           sizeof(INT32));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%04lx %04lx ",
                ((data_word >> 16) & 0xffff),
                (data_word & 0xffff));
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%04lx %04lx ",
                ((data_word >> 16) & 0xffff),
                (data_word & 0xffff));

         /* Build ASCII srting */
         result = dump_ASCII(ASCII_buffer,
                             ASCII_index,
                             &read_buffer[byte_count],
                             sizeof(INT32));
         ASCII_index = ASCII_index + sizeof(INT32);

         address = address + 1;

         byte_count = byte_count + sizeof(INT32);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_reg_half() */



/*
** This function is used to dump memory as bytes.
*/

int
dump_mem_byte(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   BYTE     data_byte;
   struct   addr_29k_t addr_29k;
   int      ASCII_index;
   char     ASCII_buffer[20];

   byte_count = 0;
   ASCII_index = 0;
   ASCII_buffer[0] = '\0';

   address_mask = 0xfffffff0;
   start_address = read_address;
   end_address = read_address + bytes_returned;
   last_print_address = (end_address + 0xf) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print out ASCII data */
      if ((address & address_mask) == address) {
         fprintf(stderr, "  %s\n", ASCII_buffer);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "  %s\n", ASCII_buffer);
         ASCII_index = 0;
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "   ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "   ");
         result = dump_ASCII(ASCII_buffer, ASCII_index,
                             (BYTE *) NULL, sizeof(BYTE));
         ASCII_index = ASCII_index + sizeof(BYTE);
         address = address + sizeof(BYTE);
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_byte,
                           &read_buffer[byte_count],
                           sizeof(BYTE));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%02x ", data_byte);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%02x ", data_byte);

         /* Build ASCII srting */
         result = dump_ASCII(ASCII_buffer,
                             ASCII_index,
                             &read_buffer[byte_count],
                             sizeof(BYTE)); 
         ASCII_index = ASCII_index + sizeof(BYTE);

         address = address + sizeof(BYTE);

         byte_count = byte_count + sizeof(BYTE);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_mem_byte() */



/*
** This function is used to dump registers as bytes.
*/

int
dump_reg_byte(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   INT32    data_word;
   struct   addr_29k_t addr_29k;
   int      ASCII_index;
   char     ASCII_buffer[20];

   byte_count = 0;
   ASCII_index = 0;
   ASCII_buffer[0] = '\0';

   address_mask = 0xfffffffc;
   start_address = read_address;
   end_address = read_address + (bytes_returned / 4);
   last_print_address = (end_address + 0x3) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print out ASCII data */
      if ((address & address_mask) == address) {
         fprintf(stderr, "  %s\n", ASCII_buffer);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "  %s\n", ASCII_buffer);
         ASCII_index = 0;
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "            ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "            ");
         result = dump_ASCII(ASCII_buffer, ASCII_index,
                             (BYTE *) NULL, sizeof(INT32));
         ASCII_index = ASCII_index + sizeof(INT32);

         address = address + 1;
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_word,
                           &read_buffer[byte_count],
                           sizeof(INT32));
         if (result != 0)
            return (EMBADADDR);

	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%02lx %02lx %02lx %02lx ",
                ((data_word >> 24) & 0xff),
                ((data_word >> 16) & 0xff),
                ((data_word >> 8) & 0xff),
                (data_word & 0xff));
         fprintf(stderr, "%02lx %02lx %02lx %02lx ",
                ((data_word >> 24) & 0xff),
                ((data_word >> 16) & 0xff),
                ((data_word >> 8) & 0xff),
                (data_word & 0xff));

         /* Build ASCII srting */
         result = dump_ASCII(ASCII_buffer,
                             ASCII_index,
                             &read_buffer[byte_count],
                             sizeof(INT32)); 
         ASCII_index = ASCII_index + sizeof(INT32);

         address = address + 1;

         byte_count = byte_count + sizeof(INT32);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_reg_byte() */



/*
** This function is used to dump memory as floats.
*/

int
dump_mem_float(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   float    data_float;
   struct   addr_29k_t addr_29k;

   byte_count = 0;

   address_mask = 0xfffffff0;
   start_address = read_address;
   end_address = read_address + bytes_returned;
   last_print_address = (end_address + 0xf) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         fprintf(stderr, "\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n");
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "               ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "               ");
         address = address + sizeof(float);
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_float,
                           &read_buffer[byte_count],
                           sizeof(float));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%+1.6e ", (double) data_float);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%+1.6e ", (double) data_float);

         address = address + sizeof(float);

         byte_count = byte_count + sizeof(float);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_mem_float() */




/*
** This function is used to dump registers as floats.
*/

int
dump_reg_float(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   float    data_float;
   struct   addr_29k_t addr_29k;

   byte_count = 0;

   address_mask = 0xfffffffc;
   start_address = read_address;
   end_address = read_address + (bytes_returned / 4);
   last_print_address = (end_address + 0x3) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         fprintf(stderr, "\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n");
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "               ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "               ");
         address = address + 1;
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_float,
                           &read_buffer[byte_count],
                           sizeof(float));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%+1.6e ", (double) data_float);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%+1.6e ", (double) data_float);

         address = address + 1;

         byte_count = byte_count + sizeof(float);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_reg_float() */




/*
** This function is used to dump memory as doubles.
*/

int
dump_mem_double(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   double   data_double;
   struct   addr_29k_t addr_29k;

   byte_count = 0;

   address_mask = 0xfffffff0;
   start_address = read_address;
   end_address = read_address + bytes_returned;
   last_print_address = (end_address + 0xf) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         fprintf(stderr, "\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n");
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "                        ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "                        ");
         address = address + sizeof(double);
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_double,
                           &read_buffer[byte_count],
                           sizeof(double));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%+1.15e ", data_double);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%+1.15e ", data_double);

         address = address + sizeof(double);

         byte_count = byte_count + sizeof(double);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_mem_double() */




/*
** This function is used to dump registers as doubles.
*/

int
dump_reg_double(memory_space, read_address, bytes_returned, read_buffer)
   INT32  memory_space;
   ADDR32 read_address;
   INT32  bytes_returned;
   BYTE   *read_buffer;
   {
   int      result;
   ADDR32   address;
   ADDR32   start_address;
   ADDR32   end_address;
   ADDR32   last_print_address;
   ADDR32   address_mask;
   INT32    byte_count;
   double   data_double;
   struct   addr_29k_t addr_29k;

   byte_count = 0;

   address_mask = 0xfffffffc;
   start_address = read_address;
   end_address = read_address + (bytes_returned / 4);
   last_print_address = (end_address + 0x3) & address_mask;
   address = start_address & address_mask;

   /*
   ** Loop while data available
   */

   while (address <= last_print_address) {

      /* Exit if address not valid */
      addr_29k.memory_space = memory_space;
      addr_29k.address = address;
      result = addr_29k_ok(&addr_29k);
      if (result != 0) {
         fprintf(stderr, "\n\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n\n");
         return (0);
         }

      /* Print address in margin */
      if (((address & address_mask) == address) &&
          (address != last_print_address)) {
         fprintf(stderr, "\n");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "\n");
         result = print_addr_29k(memory_space, address);
         if (result != 0)
            return (EMBADADDR);
         }

      /* Do leading and trailing spaces (if necessary) */
      if ((address < start_address) ||
          (address >= end_address)) {
         fprintf(stderr, "                        ");
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "                        ");
         address = address + 2;
         }

      /* Print out hex data */
      if ((address >= start_address) &&
          (address < end_address)) {

         result = get_data((BYTE *)&data_double,
                           &read_buffer[byte_count],
                           sizeof(double));
         if (result != 0)
            return (EMBADADDR);

         fprintf(stderr, "%+1.15e ", data_double);
	 if (io_config.echo_mode == (INT32) TRUE)
         fprintf(io_config.echo_file, "%+1.15e ", data_double);

         address = address + (sizeof(double) / sizeof(INT32));

         byte_count = byte_count + sizeof(double);

         }  /* end if */

      }  /* end while */

   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
   fprintf(io_config.echo_file, "\n");

   return (0);
   }  /* end dump_reg_double() */

/*
** This function fills in a buffer with a character
** representation of the dumped data.
*/

int
dump_ASCII(buffer, index, data, size)
   char    *buffer;
   int      index;
   BYTE    *data;
   int      size;
   {
   INT32    i;

   /* Do ASCII dump */
   for (i=0; i<size; i=i+1)
      if (data == NULL)
         buffer[index+i] = ' ';
      else
         if (isprint(data[i]))
            buffer[index+i] = data[i];
            else
               buffer[index+i] = '.';

   buffer[index+i] = '\0';  /* Null terminate */

   return (0);

   }  /* end dump_ASCII() */

