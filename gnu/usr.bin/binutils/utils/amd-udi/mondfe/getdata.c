static char _[] = "@(#)getdata.c	5.21 93/07/30 16:38:33, Srini, AMD.";
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
 **       This file contains functions used to parse strings into
 **       various data structures. 
 **
 *****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>
#include "memspcs.h"
#include "main.h"
#include "opcodes.h"
#include "macros.h"
#include "error.h"

#ifdef MSDOS
#include <string.h>
#else
#include <string.h>
#endif

/* Function declarations */
int get_data PARAMS(( BYTE  *out_data, BYTE  *in_data, int    size));
int set_data PARAMS((BYTE *out_data, BYTE *in_data, int size));
int get_word_decimal PARAMS((char *buffer, INT32 *data_word));
int get_double PARAMS((char *buffer, double *data_double));
int get_float PARAMS((char *buffer, float *data_float));
int get_byte PARAMS((char *buffer, BYTE  *data_byte));
int get_half PARAMS((char *buffer, INT16 *data_half));
int get_word PARAMS((char *buffer, INT32 *data_word));
int get_alias_29k PARAMS((char  *reg_str, struct addr_29k_t *addr_29k));
int get_register_29k PARAMS((char *reg_str,struct addr_29k_t *addr_29k));
int get_memory_29k PARAMS((char *memory_str, struct addr_29k_t *addr_29k, INT32 default_space));
int print_addr_29k PARAMS((INT32 memory_space, ADDR32 address));
int addr_29k_ok PARAMS((struct addr_29k_t *addr_29k));
int get_addr_29k PARAMS((char *addr_str, struct addr_29k_t *addr_29k));
int get_addr_29k_m PARAMS((char *addr_str, struct addr_29k_t *addr_29k, INT32 default_space));
void convert16 PARAMS(( BYTE *byte));
void convert32 PARAMS(( BYTE *byte));


/*
** This function is used to parse a string into the
** memory space / address pair used by the Am29000.
** the memory spaces supported are in the "memspcs.h"
** file.
**
** The strings parsed are:
**
**    lr0 to gr127  (Local registers)
**    gr0 to gr127  (Global registers)
**    sr0 to sr127  (Special registers)
**    tr0 to tr127  (TLB registers)
**    xr0 to xr32   (Coprocessor registers)
**       and
**    <hex_addr>{i|r|d}
**
** If successful, the Am29000 memory space / address pair
** pointed to by addr_29k is filled in and zero is returned.
** If unsuccessful, a -1 is returned and the values in
** addr_29k are undefined.
**
** Note:  This function expects an unpadded, lower case
**        ASCII string.
*/


int
get_addr_29k(addr_str, addr_29k)
   char   *addr_str;
   struct  addr_29k_t  *addr_29k;
   {
   /* defaults memory addresses to D_MEM */
   return(get_addr_29k_m(addr_str, addr_29k, D_MEM));
   } 


int
get_addr_29k_m(addr_str, addr_29k, default_space)
   char   *addr_str;
   struct  addr_29k_t  *addr_29k;
   INT32   default_space;	/* for default if no space given in string */
   {
   int  result;

   result = get_memory_29k(addr_str, addr_29k, default_space);

   if (result != 0)
      result = get_register_29k(addr_str, addr_29k);

   if (result != 0)
      result = get_alias_29k(addr_str, addr_29k);

   return (result);
   }  /* end get_addr_29k() */


/*
** This function is used to verify that an Am29000
** memory space / address pair contains a valid address.
** The memory spaces supported are those defined in the
** "memspcs.h" header file.
**
** The global structure "target_config" is also used to
** do range checking.
**
** If successful, a 0 is returned, otherwise -1 is
** returned.
*/


int
addr_29k_ok(addr_29k)
   struct  addr_29k_t  *addr_29k;
   {
   int     return_code;
   ADDR32  start_addr;
   ADDR32  end_addr;

   return_code = 0;

   if (addr_29k->memory_space == LOCAL_REG) {
      if (addr_29k->address > 127)
         return_code = EMBADREG;
      }
   else
   if (addr_29k->memory_space == ABSOLUTE_REG) {
	if ((addr_29k->address >= 0) && (addr_29k->address <= 255))
	   return (0);
	else
	   return (EMBADREG);
      }
   else
   if (addr_29k->memory_space == GLOBAL_REG) {
      if (PROCESSOR(target_config.processor_id) == PROC_AM29050) {
         if ( ((addr_29k->address > 3) &&
              (addr_29k->address < 64)) ||

              (addr_29k->address > 127))
            return_code = EMBADREG;
      } else {
         if ( ((addr_29k->address > 1) &&
              (addr_29k->address < 64)) ||

              (addr_29k->address > 127))
            return_code = EMBADREG;
       }
   } else /* Note:  Am29xxx procesors have different SPECIAL_REGs */
   if ((addr_29k->memory_space == SPECIAL_REG) ||
      (addr_29k->memory_space == A_SPCL_REG)) {

      if ((PROCESSOR(target_config.processor_id) == PROC_AM29030) ||
         (PROCESSOR(target_config.processor_id) == PROC_AM29240) ||
         (PROCESSOR(target_config.processor_id) == PROC_AM29035)) {

         if (((addr_29k->address > 14) &&
              (addr_29k->address < 29)) ||

             ((addr_29k->address > 30) &&
              (addr_29k->address < 128)) ||

             ((addr_29k->address > 135) &&
              (addr_29k->address < 160)) ||

             ((addr_29k->address > 162) &&
              (addr_29k->address < 164)) ||

              (addr_29k->address > 164))
            return_code = EMBADREG;
         }
      else
      if (PROCESSOR(target_config.processor_id) == PROC_AM29050) {

         if (((addr_29k->address > 26) &&
              (addr_29k->address < 128)) ||

             ((addr_29k->address > 135) &&
              (addr_29k->address < 160)) ||

              (addr_29k->address > 164))
            return_code = EMBADREG;
         }
      else	/* default */
         if (((addr_29k->address > 14) &&
              (addr_29k->address < 128)) ||

             ((addr_29k->address > 135) &&
              (addr_29k->address < 160)) ||

             ((addr_29k->address > 162) &&
              (addr_29k->address < 164)) ||

              (addr_29k->address > 164))
            return_code = EMBADREG;
      }  /* end if (SPECIAL_REG) */
   else
   if (addr_29k->memory_space == TLB_REG) {
      if (addr_29k->address > 127)
         return_code = EMBADREG;
      }
   else
   if (addr_29k->memory_space == COPROC_REG) {
      if (target_config.coprocessor != 0)
         return_code = EMBADREG;
      if (addr_29k->address > 32)
         return_code = EMBADREG;
      }
   else
   if (addr_29k->memory_space == PC_SPACE) {
      return (0);
     }
   else
   if (addr_29k->memory_space == GENERIC_SPACE) {
      return (0);
     }
   else
   if (addr_29k->memory_space == I_MEM) {
      start_addr = target_config.I_mem_start;
      end_addr = start_addr + (ADDR32) target_config.I_mem_size;
      if ((addr_29k->address < start_addr) ||
          (addr_29k->address > end_addr))
         return_code = EMBADADDR;
      }
   else
   if (addr_29k->memory_space == D_MEM) {
      start_addr = target_config.D_mem_start;
      end_addr = start_addr + (ADDR32) target_config.D_mem_size;
      if ((addr_29k->address < start_addr) ||
          (addr_29k->address > end_addr))
         return_code = EMBADADDR;
      }
   else
   if (addr_29k->memory_space == I_ROM) {
      start_addr = target_config.ROM_start;
      end_addr = start_addr + (ADDR32) target_config.ROM_size;
      if ((addr_29k->address < start_addr) ||
          (addr_29k->address > end_addr))
         return_code = EMBADADDR;
      }
   else
   if (addr_29k->memory_space == D_ROM) {
         return_code = EMBADADDR;  /* We don't use this memory space */
      }
   else
   if (addr_29k->memory_space == I_O) {
      return (0);  /* No checking on I/O space */
      }
   else
      return_code = EMBADADDR;  /* Unknown memory space */

   return (return_code);

   }  /* end addr_29k_ok() */





/*
** This function is used to print out an address.  The
** parameters are a memory_space and an address.  This
** function returns a 0 if the command was successful and
** a -1 on failure.
**
** The strings printed are:
**
**    lr0 to gr127  (Local registers)
**    gr0 to gr127  (Global registers)
**    sr0 to sr127  (Special registers)
**    tr0 to tr127  (TLB registers)
**    xr0 to xr32   (Coprocessor registers)
**    <hex_addr>    (Data memory)
**    <hex_addr>i   (instruction memory)
**       and
**    <hex_addr>r   (ROM memory)
**
*/


int
print_addr_29k(memory_space, address)
   INT32   memory_space;
   ADDR32  address;
   {
   char		buf[80];

   if (memory_space == LOCAL_REG)
      sprintf(&buf[0], "lr%03ld      ", address);
   else
   if (memory_space == ABSOLUTE_REG)
      sprintf(&buf[0], "ar%03ld      ", address);
   else
   if (memory_space == GLOBAL_REG)
      sprintf(&buf[0], "gr%03ld      ", address);
   else
   if ((memory_space == SPECIAL_REG) || (memory_space == A_SPCL_REG))
      sprintf(&buf[0], "sr%03ld      ", address);
   else
   if (memory_space == PC_SPACE)
      sprintf(&buf[0], "pc%03ld      ", address);
   else
   if (memory_space == TLB_REG)
      sprintf(&buf[0], "tr%03ld      ", address);
   else
   if (memory_space == COPROC_REG)
      sprintf(&buf[0], "xr%03ld      ", address);
   else
   if (memory_space == I_MEM)
      sprintf(&buf[0], "%08lxi  ", address);
   else
   if (memory_space == D_MEM)
      sprintf(&buf[0], "%08lx   ", address);
   else
   if (memory_space == GENERIC_SPACE)
      sprintf(&buf[0], "%08lx   ", address);
   else
   if (memory_space == I_ROM)
      sprintf(&buf[0], "%08lxr  ", address);
   else
   if (memory_space == D_ROM)
      sprintf(&buf[0], "%08ldr  ", address);
   else
   if (memory_space == I_O)
      sprintf(&buf[0], "%08lx(i/o)", address);
   else
      return (-1);

   fprintf (stderr, "%s", &buf[0]);
   if (io_config.echo_mode == (INT32) TRUE)
       fprintf (io_config.echo_file, "%s", &buf[0]);
   return (0);      
   }  /* end print_addr_29k() */



/*
** This function is used to convert a string denoting a
** 29k address into an addr_29k_t memory space / address pair.
** This function recognizes the registers described in
** get_addr_29k() above.
*/

int
get_memory_29k(memory_str, addr_29k, default_space)
   char   *memory_str;
   struct  addr_29k_t  *addr_29k;
   INT32   default_space;
   {
   int     i;
   int     fields;
   int     string_length;
   ADDR32  addr;
   char    i_r;
   char    error;

   addr_29k->memory_space = -1;
   addr_29k->address = -1;

   addr = 0;
   i_r = '\0';
   error = '\0';

   /* Get rid of leading "0x" */
   if ((strlen(memory_str) > 2) &&
       (strncmp(memory_str, "0x", 2) == 0))
      memory_str = &(memory_str[2]);

   string_length = strlen(memory_str);

   if ((string_length > 10) || (string_length < 1))
      return (EMBADADDR);

   if (memory_str[0] == '.') {/* relative offset */
     fields = sscanf(&memory_str[1], "%lx%c%c", &addr, &i_r, &error);
      addr_29k->memory_space = PC_RELATIVE;
      addr_29k->address = addr;
      return (0);
   } 

   for (i=1; i<(string_length-1); i=i+1)
    if (isxdigit(memory_str[i]) == 0)
       return (EMBADADDR);

   if ((isxdigit(memory_str[(string_length-1)]) == 0) &&
       (memory_str[(string_length-1)] != 'i') &&
       (memory_str[(string_length-1)] != 'm') &&
       (memory_str[(string_length-1)] != 'u') &&
       (memory_str[(string_length-1)] != 'p') &&
       (memory_str[(string_length-1)] != 'r'))
      return (EMBADADDR);

   fields = sscanf(memory_str, "%lx%c%c", &addr, &i_r, &error);

   addr_29k->address = addr;

   if (fields == 1) {
      addr_29k->memory_space = default_space;
      }
   else
   if (fields == 2) {
      if ((i_r == '\0') || (i_r == 'm'))
         addr_29k->memory_space = D_MEM;
      else
      if (i_r == 'r')
         addr_29k->memory_space = I_ROM;
      else
      if (i_r == 'i')
         addr_29k->memory_space = I_MEM;
      else
      if (i_r == 'p')
	 if ((target_config.processor_id & 0xf1) >= 0x50)
            addr_29k->memory_space = GENERIC_SPACE;
	 else
            addr_29k->memory_space = I_O;
      else
      if (i_r == 'u')
         addr_29k->memory_space = GENERIC_SPACE;
      else
         return (EMBADADDR);
      }
   else
      return (EMBADADDR);

   return (0);      
   }  /* end get_memory_29k() */



/*
** This function is used to convert a string denoting an
** 29k register into an addr_29k_t memory space / address pair.
*/

int
get_register_29k(reg_str, addr_29k)
   char   *reg_str;
   struct  addr_29k_t  *addr_29k;
   {
   int     fields;
   ADDR32  reg_number;
   char    error;

   addr_29k->memory_space = -1;
   addr_29k->address = -1;

   reg_number = 0;
   error = '\0';

   if (strlen(reg_str) > 8)
      return (EMBADREG);

   if (strncmp(reg_str, "lr", 2) == 0)
      addr_29k->memory_space = LOCAL_REG;
   else
   if (strncmp(reg_str, "ar", 2) == 0)
      addr_29k->memory_space = ABSOLUTE_REG;
   else
   if (strncmp(reg_str, "gr", 2) == 0)
      addr_29k->memory_space = GLOBAL_REG;
   else
   if (strncmp(reg_str, "sr", 2) == 0)
      addr_29k->memory_space = A_SPCL_REG;
   else
   if (strncmp(reg_str, "tr", 2) == 0)
      addr_29k->memory_space = TLB_REG;
   else
   if (strncmp(reg_str, "xr", 2) == 0)
     addr_29k->memory_space = COPROC_REG;

   /* Get register number */
   if (addr_29k->memory_space != -1) {
      fields = sscanf(&(reg_str[2]), "%ld%c", &reg_number, &error);
      if ((fields == 1) &&
          (error == '\0')) 
         addr_29k->address = reg_number;
         else
            addr_29k->memory_space = -1;
      }

   if (addr_29k->memory_space == -1)
      return (EMBADREG);
      else
         return (0);      
   }  /* end get_register_29k() */




/*
** This function is used to get the special register aliases
** ("cps", "vab", "ops", etc ...) in addition to the registers
** described in get_addr_29k() above.
*/

int
get_alias_29k(reg_str, addr_29k)
   char   *reg_str;
   struct  addr_29k_t  *addr_29k;
   {
   int     i;
   int     result;
   int     found;

   addr_29k->memory_space = -1;
   addr_29k->address = -1;

   if (strlen(reg_str) > 8)
      return (EMBADREG);

   /* Check for logical PC */
   if ((strcmp("pc", reg_str) == 0) ||
       (strcmp("PC", reg_str) == 0)) {
         addr_29k->memory_space = PC_SPACE;
         addr_29k->address = (ADDR32) 0;
	 return (0);
   }
   /* Search for a special register alias */
   i=0;
   found = FALSE;
   while ((i<256) && (found != TRUE)) {
      result = strcmp(spreg[i], reg_str);
      if (result == 0) {
         found = TRUE;
         addr_29k->memory_space = A_SPCL_REG;
         addr_29k->address = (ADDR32) i;
         }
      i = i + 1;
      }  /* end while */

   if (found == TRUE)
      return (0);      
      else
         return (EMBADREG);

   }  /* end get_alias_29k() */





/*
** This function is used to read in a 32 bit hex word.
** This word is input as an ASCII string and converted
** into an INT32 data_word.  If the conversion is successful,
** a zero is returned, otherwise a -1 is returned.
*/

int
get_word(buffer, data_word)
   char    *buffer;
   INT32   *data_word;
   {
   int      fields;
   char     error;

   /* No more than eight (hex) characters */
   if (strlen(buffer) > 8)
      return (EMSYNTAX);

   fields = sscanf(buffer, "%lx%c", data_word, &error);

   if (fields != 1)
      return (EMSYNTAX);

   return (0);

   }  /* end get_word() */



int
get_half(buffer, data_half)
   char    *buffer;
   INT16   *data_half;
   {
   int      fields;
   char     error;
   INT16      temp_int;

   /* No more than four (hex) characters */
   if (strlen(buffer) > 4)
      return (EMSYNTAX);

   fields = sscanf(buffer, "%hx%c", &temp_int, &error);

   if (fields != 1)
      return (EMSYNTAX);

   *data_half = (INT16) temp_int;

   return (0);

   }  /* end get_half() */


int
get_byte(buffer, data_byte)
   char    *buffer;
   BYTE    *data_byte;
   {
   int      fields;
   char     error;
   int      temp_int;

   /* No more than two (hex) characters */
   if (strlen(buffer) > 2)
      return (EMSYNTAX);

   fields = sscanf(buffer, "%x%c", &temp_int, &error);

   if (fields != 1)
      return (EMSYNTAX);

   *data_byte = (BYTE) temp_int;

   return (0);

   }  /* end get_byte() */


int
get_float(buffer, data_float)
   char    *buffer;
   float   *data_float;
   {
   int      fields;
   char     error;

   fields = sscanf(buffer, "%f%c", data_float, &error);

   if (fields != 1)
      return (EMSYNTAX);

   return (0);

   }  /* end get_float() */


int
get_double(buffer, data_double)
   char    *buffer;
   double  *data_double;
   {
   int      fields;
   char     error;

   fields = sscanf(buffer, "%lf%c", data_double, &error);

   if (fields != 1)
      return (EMSYNTAX);

   return (0);


   }  /* end get_double() */




/*
** This function is used to read in a 32 bit decimal word.
** This word is input as an ASCII string and converted
** into an INT32 data_word.  If the conversion is successful,
** a zero is returned, otherwise a -1 is returned.
** This function is very similar to get_word().
*/

int
get_word_decimal(buffer, data_word)
   char    *buffer;
   INT32   *data_word;
   {
   int      fields;
   char     error;

   /* No more than eight (hex) characters */
   if (strlen(buffer) > 8)
      return (EMSYNTAX);

   fields = sscanf(buffer, "%ld%c", data_word, &error);

   if (fields != 1)
      return (EMSYNTAX);

   return (0);

   }  /* end get_word_decimal() */


/*
** This function is used to copy data from into and out
** of the message buffers.  If necessary, endian
** conversion is performed.
*/

int
set_data(out_data, in_data, size)
   BYTE  *out_data;
   BYTE  *in_data;
   int    size;
   {
   int  i;

   if (host_config.host_endian == host_config.target_endian)
      for (i=0; i<size; i=i+1)
         out_data[i] = in_data[i];
      else
         for (i=0; i<size; i=i+1)
            out_data[i] = in_data[((size-1)-i)];

   return (0);      
   }  /* end set_data() */




/*
** This function is used to get data.
** If necessary, endian conversion is performed.
*/

int
get_data(out_data, in_data, size)
   BYTE  *out_data;
   BYTE  *in_data;
   int    size;
   {
   int  i;

   if (host_config.host_endian == host_config.target_endian)
      for (i=0; i<size; i=i+1)
         out_data[i] = in_data[i];
      else
         for (i=0; i<size; i=i+1)
            out_data[i] = in_data[((size-1)-i)];

   return (0);      
   }  /* end get_data() */



/*
** This function is used to swap the bytes in a 32 bit
** word.  This will convert "little endian" (IBM-PC / Intel)
** words to "big endian" (Sun / Motorola) words.
*/

void
convert32(byte)
   BYTE *byte;
   {
   BYTE temp;

   temp = byte[0];  /* Swap bytes 0 and 3 */
   byte[0] = byte[3];
   byte[3] = temp;
   temp = byte[1];  /* Swap bytes 1 and 2 */
   byte[1] = byte[2];
   byte[2] = temp;
   }   /* end convert32() */


/*
** This function is used to swap the bytes in a 16 bit
** word.  This will convert "little endian" (IBM-PC / Intel)
** half words to "big endian" (Sun / Motorola) half words.
*/

void
convert16(byte)
   BYTE *byte;
   {
   BYTE temp;

   temp = byte[0];  /* Swap bytes 0 and 1 */
   byte[0] = byte[1];
   byte[1] = temp;

   }   /* end convert16() */
