static char _[] = " @(#)commands.c	5.23 93/08/23 15:30:30, Srini, AMD ";
/******************************************************************************
 * Copyright 1992 Advanced Micro Devices, Inc.
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
 **       This file contains the functions associated with
 **       commands used by the main program.  In general,
 **       the commands in this file are fairly simple and
 **       were not given a source file of their own.
 *****************************************************************************
 */

#include <stdio.h>
#include <ctype.h>
#include "memspcs.h"
#include "main.h"
#include "monitor.h"
#include "macros.h"
#include "help.h"
#include "miniint.h"
#include "error.h"

#ifdef MSDOS
#include <stdlib.h>
#include <string.h>
#else
#include <strings.h>
#endif


extern	int   get_addr_29k PARAMS((char *, struct addr_29k_t *));
extern	int   addr_29k_ok PARAMS((struct addr_29k_t *));

extern	int   get_word PARAMS((char *, INT32 *));

/*
** Global variables
*/

static char *processor_name[] = {
   /*  0 */   "Am29000",
   /*  1 */   "Am29005",
   /*  2 */   "Am29050",
   /*  3 */   "Am29035",
   /*  4 */   "Am29030",
   /*  5 */   "Am29200",
   /*  6 */   "Am29240",
   /*  7 */   "Cougar",
   /*  8 */   "TBA",
   /* 9 */    "TBA"
   };
#define	NO_PROCESSOR	9

static char *coprocessor_name[] = {
   /*  0 */   "None",
   /*  1 */   "Am29027 (revision A)",
   };

static char *io_control_name[] = {
   /*  0 */   "Console controlled by target.",
   /*  1 */   "Console controlled by host."
   };



/*
** This command is used to print out the configuration
** of the system.  This includes both host and target
** configurations.
**
** This command also re-allocates the message buffers
** and the breakpoint array.  This permits this command
** to be used to re-configure the host for a new target
** without restarting the monitor.  This is useful in cases
** where the target is reset or changed.
*/

INT32
config_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int     processor;
   int     coprocessor;
   char    revision;
   char		prtbuf[256];

   INT32	retval;


   /* Get target CONFIG */
   if ((retval = Mini_config_req(&target_config, &versions_etc)) != SUCCESS) {
     return(retval);
   }; 
   /* If returned SUCCESSfully do the rest */
   


   /* Print out configuration information
   ** Note:  a -1 is no coprocessor present, 0 is an
   **        Am29027 (revision A), etc ...  To get
   **        the array index for the coprocessor_name,
   **        add one to the target_config.coprocessor.
   */

/* ----------------------------------------------------------------- */
   sprintf(&prtbuf[0], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "                MiniMON29K R 3.0 Debugger Front End.\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

/* ----------------------------------------------------------------- */
   sprintf(&prtbuf[0], "            Copyright 1993 Advanced Micro Devices, Inc.\n");
   sprintf(&prtbuf[strlen(prtbuf)], "                       Version %s (%s)\n",
          host_config.version,
          host_config.date);
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

/* ----------------------------------------------------------------- */
   sprintf(&prtbuf[0], "\n");
   if (target_config.processor_id == (UINT32) -1)
      sprintf(&prtbuf[strlen(prtbuf)], "\tProcessor type:            %s \n", processor_name[NO_PROCESSOR]);

   else {

      if ((target_config.processor_id & 0x58) == 0x58) {
	revision = (char) ('A' + (target_config.processor_id & 0x07));
        sprintf(&prtbuf[strlen(prtbuf)], "\tProcessor type:            %s (revision %c)\n", "Am29205", (char) revision);

      } else {

      processor = (int) (target_config.processor_id >> 4);
      revision = (char) ('A' + (target_config.processor_id & 0x0f));
      sprintf(&prtbuf[strlen(prtbuf)], "\tProcessor type:            %s (revision %c)\n", processor_name[processor], revision);

      }

   }

   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
/* ----------------------------------------------------------------- */
      coprocessor = (int) target_config.coprocessor + 1;
      sprintf(&prtbuf[0], "\tCoprocessor:               %s\n",
          coprocessor_name[coprocessor]);
/* ----------------------------------------------------------------- */
   if ((target_config.ROM_start == (ADDR32) -1) && 
	    		(target_config.ROM_size == (INT32) -1)) {
      sprintf(&prtbuf[strlen(prtbuf)], "\tROM range:                 Unavailable\n");
   } else {
      sprintf(&prtbuf[strlen(prtbuf)], "\tROM range:                 0x%lx to 0x%lx (%luK)\n",
          target_config.ROM_start,
          (target_config.ROM_start + (ADDR32) target_config.ROM_size - 1),
          (unsigned long) target_config.ROM_size/1024);
   }
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
/* ----------------------------------------------------------------- */
   if ((target_config.I_mem_start == (ADDR32) -1) &&
			(target_config.I_mem_size == (INT32) -1)) {
      sprintf(&prtbuf[0], "\tInstruction memory range:  Unavailable\n");
   } else {
      sprintf(&prtbuf[0], "\tInstruction memory range:  0x%lx to 0x%lx (%luK)\n",
          target_config.I_mem_start,
          (target_config.I_mem_start + (ADDR32) target_config.I_mem_size - 1),
          (unsigned long) target_config.I_mem_size/1024);
   }
/* ----------------------------------------------------------------- */
   if ((target_config.D_mem_start == (ADDR32) -1) &&
			(target_config.D_mem_size == (INT32) -1)) {
      sprintf(&prtbuf[strlen(prtbuf)], "\tData memory range:         Unavailable\n");
   } else {
      sprintf(&prtbuf[strlen(prtbuf)], "\tData memory range:         0x%lx to 0x%lx (%luK)\n",
          target_config.D_mem_start,
          (target_config.D_mem_start + (ADDR32) target_config.D_mem_size - 1),
          (unsigned long) target_config.D_mem_size/1024);
   }
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
/* ----------------------------------------------------------------- */
   sprintf(&prtbuf[0], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\t        (Enter 'h' or '?' for help)\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
/* ----------------------------------------------------------------- */

   return (SUCCESS);

   }  /* end config_cmd() */




/*
** This command is used to print out help information.
*/

INT32
help_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int   i;
   char **help_ptr;

   if (((strcmp(token[0], "h") != 0) &&
        (strcmp(token[0], "?") != 0)) ||
       (token_count > 2))
      return (EMSYNTAX);

   if (token_count == 1) {
      help_ptr = help_main;
      }
   else
   /* Print command-specific help line */
   if (token_count == 2) 
      switch (*token[1]) {
         case 'a':  help_ptr = help_a;
                    break;
         case 'b':  help_ptr = help_b;
                    break;
         case 'c':  
		    if (strcmp(token[1], "caps") == 0)
		      help_ptr = help_caps;
		    else if (strcmp(token[1], "cp") == 0)
		      help_ptr = help_cp;
		    else if (strcmp(token[1], "con") == 0)
		      help_ptr = help_con;
		    else if (strcmp(token[1], "ch0") == 0)
		      help_ptr = help_ch0;
		    else 
		      help_ptr = help_c;
                    break;
         case 'd':  help_ptr = help_d;
		    if (strcmp(token[1], "disc") == 0)
		      help_ptr = help_disc;
		    else if (strcmp(token[1], "dp") == 0)
		      help_ptr = help_dp;
                    break;
         case 'e':  help_ptr = help_e;
		    if (strcmp(token[1], "ex") == 0)
		      help_ptr = help_ex;
		    else if (strcmp(token[1], "esc") == 0)
		      help_ptr = help_esc;
		    else if (strcmp(token[1], "eon") == 0)
		      help_ptr = help_eon;
		    else if (strcmp(token[1], "eoff") == 0)
		      help_ptr = help_eon;
                    break;
         case 'f':  help_ptr = help_f;
                    break;
         case 'g':  help_ptr = help_g;
                    break;
         case 'h':  help_ptr = help_h;
                    break;
         case 'i':  help_ptr = help_i;
		    if (strcmp (token[1],"init") == 0)
		      help_ptr = help_init;
                    break;
         case 'k':  help_ptr = help_k;
                    break;
         case 'l':  help_ptr = help_l;
		    if (strcmp (token[1], "logon") == 0)
		      help_ptr = help_logon;
		    else if (strcmp (token[1], "logoff") == 0)
		      help_ptr = help_logon;
                    break;
         case 'm':  help_ptr = help_m;
                    break;
	 case 'p':  help_ptr = help_pid;
		    break;
         case 'q':  help_ptr = help_q;
		    if (strcmp(token[1], "qon") == 0)
		      help_ptr = help_qoff;
		    else if (strcmp(token[1], "qoff") == 0)
		      help_ptr = help_qoff;
                    break;
         case 'r':  help_ptr = help_r;
                    break;
         case 's':  help_ptr = help_s;
		    if (strcmp(token[1], "sid") == 0)
		      help_ptr = help_sid;
                    break;
         case 't':  help_ptr = help_t;
		    if (strcmp(token[1], "tip") == 0)
		      help_ptr = help_tip;
                    break;
         case 'x':  help_ptr = help_x;
                    break;
         case 'y':  help_ptr = help_y;
                    break;
	 case 'z':  help_ptr = help_zc;
		    if (strcmp(token[1], "ze") == 0)
		      help_ptr = help_ze;
		    else if (strcmp(token[1], "zl") == 0)
		      help_ptr = help_zl;
		    break;
         default:   help_ptr = help_main;
                    break;
         }  /* end switch */
   else
      /* Too many parameters */
      return (EMSYNTAX);


   i=0;
   while (*help_ptr[i] != '\0') {
      fprintf(stderr, "\n%s", help_ptr[i]);
      if (io_config.echo_mode == (INT32) TRUE)
         fprintf (io_config.echo_file, "\n%s", help_ptr[i]);
      i=i+1;
      }  /* end while */
   fprintf(stderr, "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf(io_config.echo_file, "\n");

   return (SUCCESS);
   }  /* end help_cmd() */



/*
** This command toggles control of the keyboard between
** TERM_USER and TERM_29K.
**
**  IMPORTANT NOTE
**   This command is no longer used.  It was an attempt to
** toggle control between the host and the target when the
** target is displaying output and accepting input.
**  The UDI methodology allows this control to be handled by the
** UDIWait procedures.  Hence, this io_toggle_cmd is not used.
**  It is left here only as an historical anomoly
**
**  The i command is now used for ix, ia, il the 2903x cashe
** which is contained in the monitor.c code.
**  END OF IMPORTANT NOTE
*/

INT32
io_toggle_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {

   if ((strcmp(token[0], "io_toggle") != 0) ||
       (token_count != 1))
      return (EMSYNTAX);

   if (io_config.io_control == TERM_29K)
      io_config.io_control = TERM_USER;
   else
   if (io_config.io_control == TERM_USER)
      io_config.io_control = TERM_29K;
   else
      return(EMFAIL);

   fprintf(stderr, "%s\n", io_control_name[io_config.io_control]);
   if (io_config.echo_mode == (INT32) TRUE)
     fprintf(io_config.echo_file, "%s\n", io_control_name[io_config.io_control]);

   return(0);

   }  /* end io_toggle_cmd() */




/*
** This command send a BREAK message to the target.  This
** should halt execution of user code.  A HALT message should
** be returned by the target.  This function deos not, however,
** wait for the HALT message.
*/

INT32
kill_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int result;
   INT32	retval;

   result = -1;
   if ((strcmp(token[0], "k") != 0) ||
       (token_count != 1))
      return (EMSYNTAX);

   if ((retval = Mini_break()) != SUCCESS) {
      return(FAILURE);
   };
   return(SUCCESS);

   }  /* end kill_cmd() */



/*
** This command send a RESET message to the target.  This
** should restart the target code.  A HALT message should
** be returned by the target.  This function deos not, however,
** wait for the HALT message.
*/

INT32
reset_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int result;
   INT32	retval;

   result = -1;
   if ((strcmp(token[0], "r") != 0) ||
       (token_count != 1))
      return (EMSYNTAX);

   if ((retval = Mini_reset_processor()) != SUCCESS) {
      return(FAILURE);
   } else
      return(SUCCESS);

   }  /* end reset_cmd() */







/*
** This command is used to display the versions of the various
** MINIMON 29K modules.  First the version of the host code and
** its date is printed from the global data structure "host_config".
** Next the montip version field and date is printed from
** the VERSION_SPACE UDIRead call.  This is an ascii zero terminated 
** field of ** less than 11 characters.
** Next the "version" field in the "target_config" data structure is
** printed.  This "version field is encoded as follows:
**
**           Bits  0 -  7:  Target debug core version
**           Bits  8 - 15:  Configuration version
**           Bits 16 - 23:  Message system version
**           Bits 24 - 31:  Communication driver version
**
** Each eight bit field is further broken up into two four bit
** fields.  The first four bits is the "release" number, the
** second is the "version" number.  This is typically printed
** as <version>.<release>.  i.e. version=2, release=6 is
** printed as "2.6".
**
*/

/*
**  The os version number is coded into the eighth word of the 
**configuration message.  It is in the lower 8 bits. 
**		Bits 0 - 7: OS version  
**
*/

INT32
version_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int comm_version;
   int message_version;
   int config_version;
   int debug_core_version;
   char tip_version[12];
   char tip_date[12];
   int os_version;    	/* eighth word of config message */
   INT32	junk;
   char	prtbuf[256];
   INT32	retval;

   if ((strcmp(token[0], "ver") != 0) ||
       (token_count != 1))
      return (EMSYNTAX);

/*  byte count is 40 because 4 bytes for target version
			     4 bytes for os version
			     12 bytes for tip version
			     12 bytes for tip date 
			     4 for msgbuf size
			     4 for max bkpts */
   if ((retval = Mini_read_req ((INT32) VERSION_SPACE,
			(ADDR32) 0,
			(INT32) 1,
			(INT16) 40,
			(INT32 *) &junk,
			(BYTE *) &(versions_etc.version),
			TRUE)) != SUCCESS)
	return (FAILURE);

   comm_version = (int) ((versions_etc.version >> 24) & 0x00ff);
   message_version = (int) ((versions_etc.version >> 16) & 0x00ff);
   config_version = (int) ((versions_etc.version >> 8)  & 0x00ff);
   debug_core_version = (int) ((versions_etc.version) & 0x00ff);
   strcpy(tip_version,versions_etc.tip_version);
   strcpy(tip_date,versions_etc.tip_date);
   os_version = (int) ((versions_etc.os_version )  & 0x00ff); 


   sprintf(&prtbuf[0], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   sprintf(&prtbuf[strlen(prtbuf)], "                  MiniMON29K R3.0\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "            Copyright 1993 Advanced Micro Devices, Inc.\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\n");

   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);
   sprintf(&prtbuf[0], "\t Host code:\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Version  %s\n",
          host_config.version);
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Date:    %s\n",
          host_config.date);
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   sprintf(&prtbuf[0], "\t Tip code:\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Version  %s\n",
          tip_version);
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Date:    %s\n",
          tip_date);
   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   sprintf(&prtbuf[0], "\t Target code:\n");
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Debug core version:            %d.%d\n",
          ((debug_core_version >> 4) & 0x0f),
           (debug_core_version & 0x0f));
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Configuration version:         %d.%d\n",
          ((config_version >> 4) & 0x0f),
           (config_version & 0x0f));
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Message system version:        %d.%d\n",
          ((message_version >> 4) & 0x0f),
           (message_version & 0x0f));
   sprintf(&prtbuf[strlen(prtbuf)], "\t\t Communication driver version:  %d.%d\n",
          ((comm_version >> 4) & 0x0f),
           (comm_version & 0x0f));

   sprintf(&prtbuf[strlen(prtbuf)], "\t\t OS system version:  	        %d.%d\n",
          ((os_version >> 4) & 0x0f),        
           (os_version & 0x0f));        


   sprintf(&prtbuf[strlen(prtbuf)], "\n");
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf (io_config.echo_file, "%s", &prtbuf[0]);
   fprintf (stderr, "%s", &prtbuf[0]);

   fprintf(stderr, "Maximum message buffer size on target: 0x%lx\n",versions_etc.max_msg_size);
   if (io_config.echo_mode == (INT32) TRUE)
     fprintf(io_config.echo_file, "Maximum message buffer size on target: 0x%lx\n",versions_etc.max_msg_size);
   fprintf(stderr, "Maximum number of breakpoints on target: %ld\n", versions_etc.max_bkpts);
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf(io_config.echo_file, "Maximum message buffer size on target: 0x%lx\n",versions_etc.max_msg_size);
   return (SUCCESS);

   }  /* end version_cmd() */



