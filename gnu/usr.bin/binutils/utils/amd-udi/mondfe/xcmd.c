static char _[] = "@(#)xcmd.c	5.20 93/07/30 16:39:02, Srini, AMD";
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
 **       This code implements a subset of the MON29K-like "x"
 **       commands.
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

INT32    xp_cmd PARAMS((char **, int));
INT32    xc_cmd PARAMS((char **, int));

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

/*
** The function below is used to implement the MON29K-like
** "x" commands.  the function below, x_cmd() is called
** in the main command loop parser of the monitor.  The
** parameters passed to this function are:
**
** token - This is an array of pointers to strings.  Each string
**         referenced by this array is a "token" of the user's
**         input, translated to lower case.
**
** token_count - This is the number of tokens in "token".
**
** This function then calls the specific "x" commands,
** such as "xp" or "xc".
*/


INT32
x_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   INT32    result;

   if (strcmp(token[0], "xp") == 0)
      result = xp_cmd(token, token_count);
   else
   if (strcmp(token[0], "xc") == 0)
      result = xc_cmd(token, token_count);
   else
      result = EMSYNTAX;

   return (result);
   }  /* end xcmd() */


/*
** This command is used to print out formatted information
** about protected special registers.  The format is borrowed
** from MON29K, and produces a full screen of data, giving
** bit fields of the various registers.
*/

INT32
xp_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   INT32  byte_count;
   int    prl;
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

   INT32	retval;
   INT32	bytes_ret;
   INT32	hostendian;
   union  {
     struct	xp_cmd_t   xp_cmd;
     char	read_buffer[XP_CMD_SZ];
   } xp_cmd_val;
   char		prtbuf[256];

   if ((strcmp(token[0], "xp") != 0) ||
       (token_count != 1))
      return (EMSYNTAX);

   /*
   ** Get data
   */

   byte_count = XP_CMD_SZ;

   /* Will the data overflow the message buffer? Done in TIP */

   hostendian = FALSE;
   if ((retval = Mini_read_req (SPECIAL_REG,
				(ADDR32) 0,
				byte_count/4,
				(INT16) 4, /* size */
				&bytes_ret,
				xp_cmd_val.read_buffer,
				hostendian)) != SUCCESS) {
	return(FAILURE);
   };
   /* The following is executed if SUCCESSful */
      vtb  = xp_cmd_val.xp_cmd.vtb;
      ops  = xp_cmd_val.xp_cmd.ops;
      cps  = xp_cmd_val.xp_cmd.cps;
      cfg  = xp_cmd_val.xp_cmd.cfg;
      cha  = xp_cmd_val.xp_cmd.cha;
      chd  = xp_cmd_val.xp_cmd.chd;
      chc  = xp_cmd_val.xp_cmd.chc;
      rbp  = xp_cmd_val.xp_cmd.rbp;
      tmc  = xp_cmd_val.xp_cmd.tmc;
      tmr  = xp_cmd_val.xp_cmd.tmr;
      pc0  = xp_cmd_val.xp_cmd.pc0;
      pc1  = xp_cmd_val.xp_cmd.pc1;
      pc2  = xp_cmd_val.xp_cmd.pc2;
      mmuc = xp_cmd_val.xp_cmd.mmuc;
      lru  = xp_cmd_val.xp_cmd.lru;

      if (host_config.host_endian != host_config.target_endian) {
         convert32((BYTE *)&vtb);
         convert32((BYTE *)&ops);
         convert32((BYTE *)&cps);
         convert32((BYTE *)&cfg);
         convert32((BYTE *)&cha);
         convert32((BYTE *)&chd);
         convert32((BYTE *)&chc);
         convert32((BYTE *)&rbp);
         convert32((BYTE *)&tmc);
         convert32((BYTE *)&tmr);
         convert32((BYTE *)&pc0);
         convert32((BYTE *)&pc1);
         convert32((BYTE *)&pc2);
         convert32((BYTE *)&mmuc);
         convert32((BYTE *)&lru);
      }


   /* Print CPS */
   sprintf(&prtbuf[0], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "       TD MM CA IP TE TP TU FZ LK RE WM PD PI SM IM DI DA\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "CPS:");
   sprintf(&prtbuf[strlen(prtbuf)], "  %3x", ((cps >> 17) & 0x01));  /* td */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 16) & 0x01));  /* mm */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 15) & 0x01));  /* ca */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 14) & 0x01));  /* ip */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 13) & 0x01));  /* te */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 12) & 0x01));  /* tp */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 11) & 0x01));  /* tu */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >> 10) & 0x01));  /* fz */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  9) & 0x01));  /* lk */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  8) & 0x01));  /* re */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  7) & 0x01));  /* wm */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  6) & 0x01));  /* pd */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  5) & 0x01));  /* pi */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  4) & 0x01));  /* sm */

   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  2) & 0x03));  /* im */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  1) & 0x01));  /* di */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cps >>  0) & 0x01));  /* da */
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   /* Print OPS */
   sprintf(&prtbuf[0], "OPS:");
   sprintf(&prtbuf[strlen(prtbuf)], "  %3x", ((ops >> 17) & 0x01));  /* td */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 16) & 0x01));  /* mm */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 15) & 0x01));  /* ca */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 14) & 0x01));  /* ip */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 13) & 0x01));  /* te */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 12) & 0x01));  /* tp */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 11) & 0x01));  /* tu */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >> 10) & 0x01));  /* fz */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  9) & 0x01));  /* lk */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  8) & 0x01));  /* re */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  7) & 0x01));  /* wm */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  6) & 0x01));  /* pd */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  5) & 0x01));  /* pi */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  4) & 0x01));  /* sm */

   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  2) & 0x03));  /* im */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  1) & 0x01));  /* di */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((ops >>  0) & 0x01));  /* da */
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   /* Get Processor Revision Number */
   prl = (int) ((cfg >> 24) & 0xff);

   /* Print VAB / CFG */
   if (PROCESSOR(prl) == PROC_AM29030) {
      sprintf(&prtbuf[0], "  VAB       CFG: PRL PMB IL ID VF BO\n");
      sprintf(&prtbuf[strlen(prtbuf)], "%08lx          ", vtb);
      sprintf(&prtbuf[strlen(prtbuf)], "%02lx", ((cfg >> 24) & 0xff));  /* prl */
      sprintf(&prtbuf[strlen(prtbuf)], "%4x", ((cfg >> 16) & 0x03));    /* pmb */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  9) & 0x03));    /* il */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  8) & 0x01));    /* id */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  4) & 0x01));    /* vf */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  2) & 0x01));    /* bo */
      sprintf(&prtbuf[strlen(prtbuf)], "\n");
      sprintf(&prtbuf[strlen(prtbuf)], "\n");
      }
   else {  /* Am29000 or Am29050 */
      sprintf(&prtbuf[0], "  VAB       CFG: PRL DW VF RV BO CP CD\n");
      sprintf(&prtbuf[strlen(prtbuf)], "%08lx          ", vtb);
      sprintf(&prtbuf[strlen(prtbuf)], "%02lx", ((cfg >> 24) & 0xff));  /* prl */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  5) & 0x01));    /* dw */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  4) & 0x01));    /* vf */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  3) & 0x01));    /* rv */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  2) & 0x01));    /* bo */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  1) & 0x01));    /* cp */
      sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((cfg >>  0) & 0x01));    /* cd */
      sprintf(&prtbuf[strlen(prtbuf)], "\n");
      sprintf(&prtbuf[strlen(prtbuf)], "\n");
      }
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   /* Print CHA / CHD / CHC */
   sprintf(&prtbuf[0], "  CHA       CHD     CHC: CE CNTL CR LS ML ST LA TF TR NN CV\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "%08lx  ", cha);     /* cha */
   sprintf(&prtbuf[strlen(prtbuf)], "%08lx       ", chd);  /* chd */
   sprintf(&prtbuf[strlen(prtbuf)], "%2x", ((chc >>  31) & 0x01));    /* ce   */
   sprintf(&prtbuf[strlen(prtbuf)], "%5x", ((chc >>  24) & 0xff));    /* cntl */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>  16) & 0xff));    /* cr   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>  15) & 0x01));    /* ls   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>  14) & 0x01));    /* ml   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>  13) & 0x01));    /* st   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>  12) & 0x01));    /* la   */

   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>  10) & 0x01));    /* tf   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>   2) & 0xff));    /* tr   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>   1) & 0x01));    /* nn   */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((chc >>   0) & 0x01));    /* cv   */
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   /* Print RBP */
   sprintf(&prtbuf[0], "RBP: BF BE BD BC BB BA B9 B8 B7 B6 B5 B4 B3 B2 B1 B0\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "    %3x", ((rbp >>  15) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>  14) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>  13) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>  12) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>  11) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>  10) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   9) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   8) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   7) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   6) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   5) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   4) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   3) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   2) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   1) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((rbp >>   0) & 0x01));
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   /* Print TMC / TMR / PC0 / PC1 / PC2 */
   sprintf(&prtbuf[0], " TCV TR: OV IN IE   TRV     PC0      PC1      PC2\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "%06lx", (tmc & 0x00ffffff));      /* tcv */
   sprintf(&prtbuf[strlen(prtbuf)], "%5x", ((tmr >> 26) & 0x01));  /* ov  */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((tmr >> 25) & 0x01));  /* in  */
   sprintf(&prtbuf[strlen(prtbuf)], "%3x", ((tmr >> 24) & 0x01));  /* ie  */
   sprintf(&prtbuf[strlen(prtbuf)], "  %06lx", (tmr & 0x00ffffff));    /* trv */
   sprintf(&prtbuf[strlen(prtbuf)], " %08lx", pc0);                /* pc0 */
   sprintf(&prtbuf[strlen(prtbuf)], " %08lx", pc1);                /* pc1 */
   sprintf(&prtbuf[strlen(prtbuf)], " %08lx", pc2);                /* pc2 */
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   /* Print MMUC / LRU */
   sprintf(&prtbuf[0], "MMU: PS PID LRU\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "     %02x", ((mmuc >>  8) & 0x03));  /* ps  */
   sprintf(&prtbuf[strlen(prtbuf)], "  %02x", (mmuc & 0xff));             /* pid */
   sprintf(&prtbuf[strlen(prtbuf)], "  %02x", (lru & 0xff));              /* lru */
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   return (0);

   }  /* end xp_cmd() */



/*
** This command is used to examine the contents of the cache
** in the Am29030.  First set 0 is printed, starting with the
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
xc_cmd(token, token_count)
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


   /* Is it an 'xc' command? */
   if (strcmp(token[0], "xc") != 0)
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

   }  /* end xc_cmd() */


