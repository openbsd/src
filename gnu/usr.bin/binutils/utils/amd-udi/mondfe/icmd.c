static char _[] = "@(#)icmd.c	5.20 93/07/30 16:38:37, Srini, AMD.";
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
 **       This code implements a subset of the MON29K-like "i"
 **       commands.  Access the 2903x cashe, ix, ia, il
 *****************************************************************************
 */


#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include "main.h"
#include "macros.h"
#include "miniint.h"
#include "memspcs.h"
#include "error.h"


#ifdef MSDOS
#include <stdlib.h>
#include <string.h>
#else
#include <string.h>
#endif

INT32    i_cmd PARAMS((char **, int));
INT32    ix_cmd PARAMS((char **, int));
INT32    il_cmd PARAMS((char **, int));

int   get_addr_29k PARAMS((char *, struct addr_29k_t *));
int   addr_29k_ok PARAMS((struct addr_29k_t *));
int   print_addr_29k PARAMS((INT32, ADDR32));
int get_word PARAMS((char *buffer, INT32 *data_word));
void convert32 PARAMS(( BYTE *byte));

void  dasm_instr PARAMS((ADDR32, struct instr_t *));

/* Variable definitions */
struct xp_cmd_t {
   INT32  vtb;
   INT32  ops;
   INT32  cps;
   INT32  cfg;
   INT32  cha;
   INT32  chd;
   INT32  chc;
   INT32  rbp;
   INT32  tmc;
   INT32  tmr;
   INT32  pc0;
   INT32  pc1;
   INT32  pc2;
   INT32  mmuc;
   INT32  lru;
};
#define	XP_CMD_SZ	15 * sizeof (INT32)
/* #define	XP_CMD_SZ	sizeof(struct xp_cmd_t) */


INT32
i_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   INT32    result;

   if (strcmp(token[0], "ix") == 0)
      result = ix_cmd(token, token_count);
   else
   if (strcmp(token[0], "il") == 0)
      result = il_cmd(token, token_count);
   else
      result = EMSYNTAX;

   return (result);
   }  /* end xcmd() */



/*
** The function below is used to implement the MON29K-like
** "i" commands.  the function below, i_cmd() is called
** in the main command loop parser of the monitor.  The
** parameters passed to this function are:
**
** token - This is an array of pointers to strings.  Each string
**         referenced by this array is a "token" of the user's
**         input, translated to lower case.
**
** token_count - This is the number of tokens in "token".
**
** This function then calls the specific "i" commands,
** such as "ix", "il" or "ia".
*/

/*
**  il
**  This command will dissasseble the contents of the cache  
** This command is used to examine the contents of the cache
** in the Am29030.  First set 0 is printed, starting with the
**  This command will dissasseble the contents of the cache  
** tag, followed by a disassembly of four instructions in
** the set.  Set 1 for the line follows similarly.
**
** The data comes in from the READ_ACK message in the following
** order:
**
**            tag      (data[0-3]    (set 0)
**         instr1      (data[4-7]
**         instr1      (data[8-11]
**         instr1      (data[12-15]
**         instr1      (data[16-19]
**
**            tag      (data[20-23]  (set 1)
**         instr1      (data[24-27]
**         instr1      (data[28-31]
**         instr1      (data[32-35]
**         instr1      (data[36-39]
*/

INT32
il_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   static INT32  memory_space=I_CACHE;
   static ADDR32 cache_line=0;
   static INT32  byte_count=(10*sizeof(INST32));
   static INT32  count=1;
   ADDR32 address;
   INT32  i;
   int    j;
   int    set;
   int    index;
   int    result;
   struct instr_t instr;
   INT32  cache_line_start;
   INT32  cache_line_end;

   INT32	retval;
   INT32	bytes_ret;
   INT32	host_endian;
   BYTE		read_buffer[10*sizeof(INST32)];
   char		prtbuf[256];


   /* Is it an 'il' command? */
   if (strcmp(token[0], "il") != 0)
      return (EMSYNTAX);

   /*
   ** Parse parameters
   */

   if (token_count == 1) {
      cache_line = cache_line + count;
      }
   else
   if (token_count == 2) {
      result = get_word(token[1], &cache_line_start);
      if (result != 0)
         return (EMSYNTAX);
      if ((cache_line_start < 0) ||
          (cache_line_start >255))
         return (EMBADADDR);
      cache_line = cache_line_start;
      }
   else
   if (token_count == 3) {
      /* Get first cache line to be dumped */
      result = get_word(token[1], &cache_line_start);
      if (result != 0)
         return (EMSYNTAX);
      if ((cache_line_start < 0) ||
          (cache_line_start > 255))
         return (EMBADADDR);
      /* Get last cache line to be dumped */
      result = get_word(token[2], &cache_line_end);
      if (result != 0)
         return (EMSYNTAX);
      if ((cache_line_end < 0) ||
          (cache_line_end > 255))
         return (EMBADADDR);
      if (cache_line_start > cache_line_end)
         return (EMBADADDR);
      cache_line = cache_line_start;
      count = (cache_line_end - cache_line_start) + 1;
      }
   else
   /* Too many args */
      return (EMSYNTAX);

   i = 0;
   while (i < count) {

      host_endian = FALSE;
      if ((retval = Mini_read_req(memory_space,
				  (cache_line + i),
				  byte_count/4,
				  (INT16) 4, /* size */
				  &bytes_ret,
				  read_buffer,
				  host_endian)) != SUCCESS) {
	 return(FAILURE);
      };
      /* The following is executed if SUCCESSful */

      for (set=0; set<2; set++) {

         /* Print out formatted address tag and status information */
         index = (set * 20);
         sprintf(&prtbuf[0], "\n");
         sprintf(&prtbuf[strlen(prtbuf)], "Cache line 0x%lx, set %d.\n", (int) (cache_line+i), set);
         sprintf(&prtbuf[strlen(prtbuf)], "\n");
         if (io_config.echo_mode == (INT32) TRUE)
            fprintf (io_config.echo_file, "%s", &prtbuf[0]);
         fprintf (stderr, "%s", &prtbuf[0]);
         sprintf(&prtbuf[0], "IATAG  V  P US\n");
         if (io_config.echo_mode == (INT32) TRUE)
            fprintf (io_config.echo_file, "%s", &prtbuf[0]);
         fprintf (stderr, "%s", &prtbuf[0]);
         sprintf(&prtbuf[0], "%02x%02x%1x  %1x  %1x  %1x\n",
                read_buffer[index],
                read_buffer[index + 1],
                ((read_buffer[index + 2] >> 4) & 0x0f),
                ((read_buffer[index + 3] >> 2) & 0x01),
                ((read_buffer[index + 3] >> 1) & 0x01),
                (read_buffer[index + 3] & 0x01));
         sprintf(&prtbuf[strlen(prtbuf)], "\n");
   	 if (io_config.echo_mode == (INT32) TRUE)
      	   fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   	 fprintf (stderr, "%s", &prtbuf[0]);

         /* Address = IATAG + line_number + <16 byte adddress> */
         address = ((read_buffer[index] << 24) |
                    (read_buffer[index + 1] << 16) |
                    (read_buffer[index + 2] << 8) |
                    ((cache_line+i) << 4));

         /* Disassemble four words */
         for (j=0; j<4; j=j+1) {
            index = (set * 20) + ((j+1) * sizeof(INT32));
            instr.op = read_buffer[index];
            instr.c = read_buffer[index + 1];
            instr.a = read_buffer[index + 2];
            instr.b = read_buffer[index + 3];

            /* Print address of instruction (in hex) */
            address = (address & 0xfffffff0);  /* Clear low four bits */
            address = (address | (j << 2));
            fprintf(stderr, "%08lx    ", address);
	    if (io_config.echo_mode == (INT32) TRUE)
               fprintf(io_config.echo_file, "%08lx    ", address);

            /* Print instruction (in hex) */
	    if (io_config.echo_mode == (INT32) TRUE)
               fprintf(io_config.echo_file, "%02x%02x%02x%02x    ", instr.op, instr.c,
                   instr.a, instr.b);
            fprintf(stderr, "%02x%02x%02x%02x    ", instr.op, instr.c,
                   instr.a, instr.b);

            /* Disassemble instruction */
            dasm_instr(address, &instr);
            fprintf(stderr, "\n");
	    if (io_config.echo_mode == (INT32) TRUE)
               fprintf(io_config.echo_file, "\n");

            }  /* end for(j) */

         fprintf(stderr, "\n");
	 if (io_config.echo_mode == (INT32) TRUE)
           fprintf(io_config.echo_file, "\n");

         }  /* end for(set) */

      i = i + 1;

      }  /* end while loop */

   return (0);

   }  /* end il_cmd() */



/*
**  ix
**  This command will dump the contents of the cache in hex 
** This command is used to examine the contents of the cache
** in the Am29030.  
** First set 0 is printed, starting with the
** tag, followed by a disassembly of four instructions in
** the set.  Set 1 for the line follows similarly.
**
** The data comes in from the READ_ACK message in the following
** order:
**
**            tag      (data[0-3]    (set 0)
**         instr1      (data[4-7]
**         instr1      (data[8-11]
**         instr1      (data[12-15]
**         instr1      (data[16-19]
**
**            tag      (data[20-23]  (set 1)
**         instr1      (data[24-27]
**         instr1      (data[28-31]
**         instr1      (data[32-35]
**         instr1      (data[36-39]
*/

INT32
ix_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   static INT32  memory_space=I_CACHE;
   static ADDR32 cache_line=0;
   static INT32  byte_count=(10*sizeof(INST32));
   static INT32  count=1;
   ADDR32 address;
   INT32  i;
   int    j;
   int    set;
   int    index;
   int    result;
   struct instr_t instr;
   INT32  cache_line_start;
   INT32  cache_line_end;

   INT32	retval;
   INT32	bytes_ret;
   INT32	host_endian;
   BYTE		read_buffer[10*sizeof(INST32)];
   char		prtbuf[256];


   /* Is it an 'ix' command? */
   if (strcmp(token[0], "ix") != 0)
      return (EMSYNTAX);

   /*
   ** Parse parameters
   */
   if (token_count == 1) {
      cache_line = cache_line + count;
      }
   else
   if (token_count == 2) {
      result = get_word(token[1], &cache_line_start);
      if (result != 0)
         return (EMSYNTAX);
      if ((cache_line_start < 0) ||
          (cache_line_start >255))
         return (EMBADADDR);
      cache_line = cache_line_start;
      }
   else
   if (token_count == 3) {
      /* Get first cache line to be dumped */
      result = get_word(token[1], &cache_line_start);
      if (result != 0)
         return (EMSYNTAX);
      if ((cache_line_start < 0) ||
          (cache_line_start > 255))
         return (EMBADADDR);
      /* Get last cache line to be dumped */
      result = get_word(token[2], &cache_line_end);
      if (result != 0)
         return (EMSYNTAX);
      if ((cache_line_end < 0) ||
          (cache_line_end > 255))
         return (EMBADADDR);
      if (cache_line_start > cache_line_end)
         return (EMBADADDR);
      cache_line = cache_line_start;
      count = (cache_line_end - cache_line_start) + 1;
      }
   else
   /* Too many args */
      return (EMSYNTAX);

   i = 0;
   while (i < count) {

      host_endian = FALSE;
      if ((retval = Mini_read_req(memory_space,
				  (cache_line + i),
				  byte_count/4,
				  (INT16) 4, /* size */
				  &bytes_ret,
				  read_buffer,
				  host_endian)) != SUCCESS) {
	 return(FAILURE);
      };
      /* The following is executed if SUCCESSful */

      for (set=0; set<2; set++) {

         /* Print out formatted address tag and status information */
         index = (set * 20);
         sprintf(&prtbuf[0], "\n");
         sprintf(&prtbuf[strlen(prtbuf)], "Cache line 0x%lx, set %d.\n", (int) (cache_line+i), set);
         sprintf(&prtbuf[strlen(prtbuf)], "\n");
         if (io_config.echo_mode == (INT32) TRUE)
            fprintf (io_config.echo_file, "%s", &prtbuf[0]);
         fprintf (stderr, "%s", &prtbuf[0]);
         sprintf(&prtbuf[0], "IATAG  V  P US\n");
         if (io_config.echo_mode == (INT32) TRUE)
            fprintf (io_config.echo_file, "%s", &prtbuf[0]);
         fprintf (stderr, "%s", &prtbuf[0]);
         sprintf(&prtbuf[0], "%02x%02x%1x  %1x  %1x  %1x\n",
                read_buffer[index],
                read_buffer[index + 1],
                ((read_buffer[index + 2] >> 4) & 0x0f),
                ((read_buffer[index + 3] >> 2) & 0x01),
                ((read_buffer[index + 3] >> 1) & 0x01),
                (read_buffer[index + 3] & 0x01));
         sprintf(&prtbuf[strlen(prtbuf)], "\n");
   	 if (io_config.echo_mode == (INT32) TRUE)
      	   fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   	 fprintf (stderr, "%s", &prtbuf[0]);

         /* Address = IATAG + line_number + <16 byte adddress> */
         address = ((read_buffer[index] << 24) |
                    (read_buffer[index + 1] << 16) |
                    (read_buffer[index + 2] << 8) |
                    ((cache_line+i) << 4));

         /* Disassemble four words */
         for (j=0; j<4; j=j+1) {
            index = (set * 20) + ((j+1) * sizeof(INT32));
            instr.op = read_buffer[index];
            instr.c = read_buffer[index + 1];
            instr.a = read_buffer[index + 2];
            instr.b = read_buffer[index + 3];

            /* Print address of instruction (in hex) */
            address = (address & 0xfffffff0);  /* Clear low four bits */
            address = (address | (j << 2));
            fprintf(stderr, "%08lx    ", address);
	    if (io_config.echo_mode == (INT32) TRUE)
               fprintf(io_config.echo_file, "%08lx    ", address);

            /* Print instruction (in hex) */
	    if (io_config.echo_mode == (INT32) TRUE)
               fprintf(io_config.echo_file, "%02x%02x%02x%02x    ", instr.op, instr.c,
                   instr.a, instr.b);
            fprintf(stderr, "%02x%02x%02x%02x    ", instr.op, instr.c,
                   instr.a, instr.b);

            /* Disassemble instruction */
            dasm_instr(address, &instr);
            fprintf(stderr, "\n");
	    if (io_config.echo_mode == (INT32) TRUE)
               fprintf(io_config.echo_file, "\n");

            }  /* end for(j) */

         fprintf(stderr, "\n");
	 if (io_config.echo_mode == (INT32) TRUE)
           fprintf(io_config.echo_file, "\n");

         }  /* end for(set) */

      i = i + 1;

      }  /* end while loop */

   return (0);

   }  /* end ix_cmd() */


