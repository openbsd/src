static char _[] = "@(#)monitor.c	5.28 93/11/02 11:46:54, Srini, AMD.";
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
 * This module implements the monitor command interpreter.
 *****************************************************************************
 */

#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#include  <signal.h>
#ifdef	MSDOS
#include  <stdlib.h>
#include  <conio.h>
#include  <io.h>
#else
#include  <fcntl.h>
#include  <termio.h>
/* #include   <sys/ioctl.h> */
#endif
#include  "monitor.h"
#include  "main.h"
#include  "memspcs.h"
#include  "error.h"
#include  "miniint.h"

/* Function declarations */
extern	void	Mini_Ctrl_C_Handler PARAMS((int));
extern	void	Mini_loop PARAMS((void));
extern	INT32	Mini_go_forever PARAMS((void));
extern	void	Mini_poll_kbd PARAMS((char  *cmd_buffer, int size, int mode));
extern	int	Mini_cmdfile_input PARAMS((char  *cmd_buffer, int size));
extern	int   	tokenize_cmd PARAMS((char *, char **));
extern 	void 	lcase_tokens PARAMS((char **, int));
extern	int   	get_word PARAMS((char *, INT32 *));
extern	void	PrintTrapMsg PARAMS((int trapnum));
extern	void	display_msg PARAMS((void));
extern	void	display_termuser PARAMS((void));
extern	void	display_term29k PARAMS((void));
extern	INT32	get_pc_addrs PARAMS((ADDR32 *pc1, ADDR32 *cps));
extern	INT32	get_pc1_inst PARAMS((ADDR32 cps, ADDR32 pc1, BYTE *inst));
extern	void 	dasm_instr PARAMS((ADDR32, struct instr_t *));
extern	void	convert32 PARAMS((BYTE *));
extern	INT32	Mini_send_init_info PARAMS((INIT_INFO *init));

static	int	FindCmdIndx PARAMS((char *string));

/* Globals */
GLOBAL 	struct bkpt_t 	*bkpt_table;

GLOBAL 	char   		cmd_buffer[BUFFER_SIZE];
GLOBAL 	char   		tip_cmd_buffer[256];
GLOBAL	int		GoCmdFlag=0;

#define	TOGGLE_CHAR	(io_config.io_toggle_char)

/* Following three vars to be used in stdin/stdout/stderr funcs */
#define	IO_BUFSIZE	1024
static	char		io_buffer[IO_BUFSIZE]; 
static	INT32	 	io_bufsize;
static	INT32		io_count_done;

static	INT32		ProcessorState;
static	int		GrossState;
static	INT32		exit_loop;
static	int		CtrlCHit=0;
static	int		BlockMode;
/* These modes are defined in montip, udi2mtip.c file */
#define	TIP_COOKED	0x0
#define	TIP_RAW		0x1
#define	TIP_CBREAK	0x2
#define	TIP_ECHO	0x4
#define	TIP_ASYNC	0x8
#define	TIP_NBLOCK	0x10
static	INT32		TipStdinMode = TIP_COOKED;	/* initial */
#ifndef	MSDOS
struct termio OldTermbuf, NewTermbuf;	/* STDIN, Channel0 */
#endif /* MSDOS */

/* Monitor command table */
struct	MonitorCommands_t {
  char		*CmdString; /* Maximum length */
  INT32		(*CmdFn) PARAMS((char **, int));
};

static	struct MonitorCommands_t MonitorCommands[] = {
"a", asm_cmd,
"attach", set_sessionid_cmd,
"b", bkpt_cmd,
"bc", bkpt_cmd,
"b050", bkpt_cmd,
"b050v", bkpt_cmd,
"b050p", bkpt_cmd,
"c", config_cmd,
"caps", capab_cmd,
"cp", create_proc_cmd,
"con", connect_cmd,
"ch0", channel0_cmd,
"d", dump_cmd,
"ex", exit_conn_cmd,
"dw", dump_cmd,
"dh", dump_cmd,
"db", dump_cmd,
"df", dump_cmd,
"dd", dump_cmd,
"dp", destroy_proc_cmd,
"disc", disconnect_cmd,
"detach", disconnect_cmd,
"esc", escape_cmd,
"eon", echomode_on,
"eoff", echomode_off,
"f", fill_cmd,
"fw", fill_cmd,
"fh", fill_cmd,
"fb", fill_cmd,
"ff", fill_cmd,
"fd", fill_cmd,
"fs", fill_cmd,
"g", go_cmd,
"h", help_cmd,
"ix", ix_cmd,
"il", il_cmd,
"init", init_proc_cmd,
"k", kill_cmd,
"l", dasm_cmd,
"logon", logon_cmd,
"logoff", logoff_cmd,
"m", move_cmd,
"pid", set_pid_cmd,
"q", quit_cmd,
"qoff", quietmode_off,
"qon", quietmode_on,
"r", reset_cmd,
"s", set_cmd,
"sw", set_cmd,
"sh", set_cmd,
"sb", set_cmd,
"sf", set_cmd,
"sd", set_cmd,
"sid", set_sessionid_cmd,
"t", trace_cmd,
"target", connect_cmd,
"tip", tip_cmd,
"ver", version_cmd,
"xp", xp_cmd,
"xc", xc_cmd,
"y", yank_cmd,
"zc", cmdfile_cmd,
"ze", echofile_cmd,
"zl", set_logfile,
"?", help_cmd,
"|", dummy_cmd,
"", dummy_cmd,
NULL
};

/* Trap Messages */
static char	*TrapMsg[] = {
"Illegal Opcode",
"Unaligned Access",
"Out of Range",
"Coprocessor Not Present",
"Coprocessor Exception",
"Protection Violation",
"Instruction Access Exception",
"Data Access Exception",
"User-Mode Instruction TLB Miss",
"User-Mode Data TLB Miss",
"Supervisor-Mode Instruction TLB Miss",
"Supervisor-Mode Data TLB Miss",
"Instruction TLB Protection Violation",
"Data TLB Protection Violation",
"Timer",
"Trace",
"INTR0",
"INTR1",
"INTR2",
"INTR3",
"TRAP0",
"TRAP1",
"Floating-Point Exception"
};

void
Mini_monitor()
{

  /* Initialize breakpoint table */

  bkpt_table = NULL;
  GrossState = NOTEXECUTING;

   /*
    * Start with the user being the terminal controller.
    */
    io_config.io_control = TERM_USER;
    io_config.target_running = FALSE;

#ifndef	MSDOS
   ioctl (fileno(stdin), TCGETA, &OldTermbuf);	/* Initial settings */
#endif

   /*
    * Define Ctrl-U as the io_toggle_char as default.
    */
    io_config.io_toggle_char = (BYTE) 21;

   /*
   ** Open cmd file (if necessary)
   */

   if (io_config.cmd_file_io == TRUE) {  /* TRUE if -c option given */
      io_config.cmd_file = fopen(io_config.cmd_filename, "r");
        if (io_config.cmd_file == NULL) {
            warning (EMCMDOPEN);
            io_config.cmd_file_io = FALSE;
        } else {
	  /* MON_STDIN is command file */
	  MON_STDIN = fileno(io_config.cmd_file); /* set MON_STDIN */
	};
   }

   /*
   ** Open log file, if no command file given.
   */

   if (io_config.log_mode == (INT32) TRUE) { /* -log option given */
     if (io_config.log_filename) {
       io_config.log_file = fopen(io_config.log_filename, "w");
       if (io_config.log_file == NULL) {
          io_config.log_mode = (INT32) FALSE;
          warning(EMLOGOPEN);
       }
     } else {
       io_config.log_mode = (INT32) FALSE;
       warning(EMLOGOPEN);
     }
   }

   /* Install ctrl-C handler */

   if (signal (SIGINT, Mini_Ctrl_C_Handler) == SIG_ERR) {
     fprintf(stderr, "Ctrl-C handler not installed.\n"); /* warning */
     if (io_config.echo_mode == (INT32) TRUE)
        fprintf(io_config.echo_file, "Ctrl-C handler not installed.\n"); /* warning */
   }
  /* Get into monitor loop */

  Mini_loop();

}

void
Mini_loop()
{
   INT32	retval;
   int    	token_count;
   char  	*token[MAX_TOKENS];
   int		Indx;
 
   exit_loop = FALSE;
   CtrlCHit = 0;
   /*
   ** Enter command interpreter loop
   */

   fprintf(stderr, "%s>", ProgramName);
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf(io_config.echo_file, "%s>", ProgramName);

   BlockMode = BLOCK;	/* wait for a user command */
   do {

      if (CtrlCHit) {
	CtrlCHit = 0;
   	/* Print a prompt */
   	fprintf(stderr, "\n%s>", ProgramName);
   	if (io_config.echo_mode == (INT32) TRUE)
      	  fprintf(io_config.echo_file, "\n%s>", ProgramName);
      }
      /*
      ** If the target was set to run, get its current status.
      */
      if (Mini_get_target_stats((INT32) udi_waittime, &ProcessorState) != SUCCESS) {
         Mini_TIP_DestroyProc();
         Mini_TIP_exit();
	 fatal_error(EMFATAL);
      };
      GrossState = (int) (ProcessorState & 0xFF);
      switch (GrossState) {
	case	NOTEXECUTING:  /* do nothing */
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		BlockMode = BLOCK;	/* wait for a user command */
		break;
	case	EXITED:  /* do nothing */
		if (GoCmdFlag) {
		 GoCmdFlag = 0;
#ifndef	MSDOS
                 ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
		 if (!QuietMode) {
		    fprintf (stderr, "Process exited with 0x%lx\n", 
					(ProcessorState >> 8));
     		    if (io_config.echo_mode == (INT32) TRUE)
		       fprintf (io_config.echo_file, "Process exited with 0x%lx\n", 
					(ProcessorState >> 8));
		 }
		 fprintf (stderr, "%s>", ProgramName);
     		 if (io_config.echo_mode == (INT32) TRUE)
		    fprintf (io_config.echo_file, "%s> ", ProgramName);
		}
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		BlockMode = BLOCK; /* wait for a user command */
		break;
	case	RUNNING:	/* any request from target? */
     		io_config.target_running = TRUE;
		BlockMode = NONBLOCK; /* return immediately */
		break;
	case	STOPPED:
		io_config.io_control=TERM_USER;
     		io_config.target_running = TRUE;
		if (GoCmdFlag) {
		   GoCmdFlag = 0;
#ifndef	MSDOS
                   ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
		   fprintf(stderr, "Execution stopped at ");
     		   if (io_config.echo_mode == (INT32) TRUE)
		      fprintf(io_config.echo_file, "Execution stopped at ");
		   display_msg();
		}
		BlockMode = BLOCK; /* wait for next user command */
		break;
	case	BREAK:
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		if (GoCmdFlag) {
		   GoCmdFlag = 0;
#ifndef	MSDOS
                   ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
		   fprintf(stderr, "Breakpoint hit at ");
     		   if (io_config.echo_mode == (INT32) TRUE)
		      fprintf(io_config.echo_file, "Breakpoint hit at ");
		   display_msg();
		}
		BlockMode = BLOCK; /* wait for next user command */
		break;
	case	STEPPED:
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		if (GoCmdFlag) {
		   GoCmdFlag = 0;
#ifndef	MSDOS
                   ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
		   fprintf(stderr, "Stepping... Execution stopped at ");
     		   if (io_config.echo_mode == (INT32) TRUE)
		      fprintf(io_config.echo_file, "Stepping...Execution stopped at ");
		   display_msg();
		}
		BlockMode = BLOCK; /* wait for next user command */
		break;
	case	WAITING:
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		break;
	case	HALTED:
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		if (GoCmdFlag) {
		   GoCmdFlag = 0;
#ifndef	MSDOS
                   ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
		   fprintf(stderr, "Execution halted at ");
     		   if (io_config.echo_mode == (INT32) TRUE)
		      fprintf(io_config.echo_file, "Execution halted at ");
		   display_msg();
		}
		BlockMode = BLOCK; /* wait for next user command */
		break;
	case	WARNED:
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		break;
	case	TRAPPED:
		io_config.io_control=TERM_USER;
     		io_config.target_running = FALSE;
		if (GoCmdFlag) {
		   GoCmdFlag = 0;
#ifndef	MSDOS
                   ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
		   PrintTrapMsg((int) (ProcessorState >> 8));
		   display_msg();
		}
		BlockMode = BLOCK; /* wait for next user command */
		break;
	case	STDOUT_READY:
		io_bufsize = 0;
		io_count_done = (INT32) 0;
		do {
		  Mini_get_stdout(io_buffer, IO_BUFSIZE, &io_count_done);
		  write(MON_STDOUT, &io_buffer[0], (int) io_count_done);
		  if (io_config.echo_mode == (INT32) TRUE) {
		   fflush (io_config.echo_file);
		   write (fileno(io_config.echo_file), &io_buffer[0], (int) io_count_done);
		  }
		} while (io_count_done == (INT32) IO_BUFSIZE);
		break;
	case	STDERR_READY:
		io_bufsize = 0;
		io_count_done = (INT32) 0;
		do {
		  Mini_get_stderr(io_buffer, IO_BUFSIZE, &io_count_done);
		  write(MON_STDERR, &io_buffer[0], (int) io_count_done);
		  if (io_config.echo_mode == (INT32) TRUE) {
		     fflush (io_config.echo_file);
		     write (fileno(io_config.echo_file), &io_buffer[0], (int) io_count_done);
		  }
		} while (io_count_done == (INT32) IO_BUFSIZE);
		break;
	case	STDIN_NEEDED:
		/* Line buffered reads only */
		if (io_config.cmd_file_io == TRUE) { /* read from command file */
		   if (Mini_cmdfile_input(io_buffer, IO_BUFSIZE) == SUCCESS) {
		      io_bufsize = strlen(io_buffer);
		      fprintf(stderr, "%s", io_buffer); /* echo */
     		      if (io_config.echo_mode == (INT32) TRUE)
		          fprintf(io_config.echo_file, "%s", io_buffer); /* echo */
		   } else { /* read from terminal */
		     io_bufsize = read( fileno(stdin), io_buffer, IO_BUFSIZE );
		   }
		} else {
		   io_bufsize = read( fileno(stdin), io_buffer, IO_BUFSIZE );
		};
		if (io_bufsize < 0)
		{
		   fprintf(stderr, "fatal error reading from stdin\n");
     		   if (io_config.echo_mode == (INT32) TRUE)
		      fprintf(io_config.echo_file, "fatal error reading from stdin\n");
		}
		if (io_config.echo_mode == (INT32) TRUE) {
		  write (fileno(io_config.echo_file), &io_buffer[0], (int) io_bufsize);
		  fflush (io_config.echo_file);
		}
		Mini_put_stdin(io_buffer, io_bufsize, &io_count_done);
		break;
	case	STDINMODEX:
		/* call TIP to get StdinMode */
		Mini_stdin_mode_x((INT32 *)&TipStdinMode);
		if (TipStdinMode & TIP_NBLOCK) 
		  io_config.io_control = TERM_29K;
		else if (TipStdinMode & TIP_ASYNC)
		  io_config.io_control = TERM_29K;
		else if (TipStdinMode == TIP_COOKED)
		  io_config.io_control = TERM_USER;
		else {
		  fprintf(stderr, "DFEWARNING: TIP Requested Stdin Mode Not Supported.\n");
		  fprintf(stderr, "DFEWARNING: Using default mode.\n");
		  TipStdinMode = TIP_COOKED;
		  io_config.io_control = TERM_USER;
		}

		if (io_config.io_control == TERM_29K)
		  display_term29k();
		break;

	default:
		break;
      }; /* end switch */


      /*
      ** Check for keyboard input from command file first, then keyboard.
      */
      if (io_config.io_control == TERM_USER)  {
        if (io_config.target_running == FALSE) {
	  if (io_config.cmd_ready == FALSE) { /* Get a new user command */
             if (io_config.cmd_file_io == TRUE) { /* try command file first*/
	        if (Mini_cmdfile_input(cmd_buffer, BUFFER_SIZE) == SUCCESS) {
                   fprintf(stderr, "%s", cmd_buffer); 
		   io_config.cmd_ready = TRUE;
		} else {
                   Mini_poll_kbd(cmd_buffer, BUFFER_SIZE, BlockMode);
		}
	     } else { /* keyboard */
		/* Mini_poll_kbd function sets io_config.cmd_ready */
                Mini_poll_kbd(cmd_buffer, BUFFER_SIZE, BlockMode);
	     }
	  }
	} else {
                Mini_poll_kbd(cmd_buffer, BUFFER_SIZE, BlockMode);
	}
      } else if (io_config.io_control == TERM_29K) {
	if ((GrossState == RUNNING) || GoCmdFlag)
	   Mini_poll_channel0();	/* non-blocking */
      } else {
	fprintf(stderr, "fatal error: Don't know who is controlling the terminal!\n");
	return;
      }


      if (io_config.cmd_ready == TRUE) { /* if there is a command in buffer */
#ifdef	MSDOS
	     if (io_config.log_mode == (INT32) TRUE)  /* make a log file */
                fprintf(io_config.log_file, "%s\n", cmd_buffer);
     	     if (io_config.echo_mode == (INT32) TRUE)
                fprintf(io_config.echo_file, "%s\n", cmd_buffer);
#else
	     if (io_config.log_mode == (INT32) TRUE)  /* make a log file */
                fprintf(io_config.log_file, "%s", cmd_buffer);
     	     if (io_config.echo_mode == (INT32) TRUE)
                fprintf(io_config.echo_file, "%s", cmd_buffer);
#endif
      	     /*
      	     ** Parse command
      	     */

             token_count = tokenize_cmd(cmd_buffer, token);
             /* Convert first character (command) to lcase */
             if (isupper(*token[0]))
                (*token[0]) = (char) tolower(*token[0]);

             /* If anything but a y or z command, convert to lower case */
             if ( ((*token[0]) != 'y') &&
		  ((*token[0]) != 'z') )
                lcase_tokens(token, token_count);

	     if ((Indx = FindCmdIndx(token[0])) != (int) FAILURE)
	       io_config.cmd_ready = TRUE;
	     else {
	       warning(EMNOSUCHCMD);
               /* Print a prompt */
   	       fprintf(stderr, "\n%s>", ProgramName);
     	       if (io_config.echo_mode == (INT32) TRUE)
   	          fprintf(io_config.echo_file, "\n%s>", ProgramName);
	       io_config.cmd_ready = FALSE; /* nothing to execute */
	     }
      }

      /*
      ** Execute command
      */

      if (io_config.cmd_ready == TRUE) {
	 retval = MonitorCommands[Indx].CmdFn(token, token_count);
         io_config.cmd_ready = FALSE;
	 if (retval == FAILURE) {
	    fprintf(stderr, "Command failed\n");
     	    if (io_config.echo_mode == (INT32) TRUE)
	       fprintf(io_config.echo_file, "Command failed\n");
	 } else if (retval != SUCCESS) {
	    warning(retval);
	 };
         /* Print a prompt */
	 if (io_config.io_control == TERM_USER) {
   	   fprintf(stderr, "%s>", ProgramName);
     	   if (io_config.echo_mode == (INT32) TRUE)
   	      fprintf(io_config.echo_file, "%s>", ProgramName);
	 } else {
		  display_term29k();
	 }
      }  /* if cmd ready */

   } while (exit_loop != TRUE);  /* end of do-while */

   /* Close log file */
   if (io_config.log_mode == (INT32) TRUE)
      (void) fclose(io_config.log_file);

   if (bkpt_table != NULL)
       (void) free((char *) bkpt_table);
}

/*
** This function takes in a string and produces a lower case,
** " argv - argc" style array.  Then number of elements in the
** array is returned.
*/

int
tokenize_cmd(cmd, token)
   char  *cmd;
   char  *token[];
   {
   int  token_count;

   /* Break input into tokens */
   token_count = 0;
   token[0] = cmd;

   if (cmd[0] != '\0') {
      token[token_count] = strtok(cmd, " \t,;\n\r");

      if (token[token_count] != NULL) {
         do {
            token_count = token_count + 1;
            token[token_count] = strtok((char *) NULL, " \t,;\n\r");
            } while ((token[token_count] != NULL) &&
                     (token_count < MAX_TOKENS));
         }
      else {
         token[0] = cmd;
         *token[0] = '\0';
         }
      }

   return (token_count);

   }  /* end tokenize_cmd() */



/*
** This function is used to convert a list of tokens
** to all lower case letters.
*/

void
lcase_tokens(token, token_count)
   char *token[MAX_TOKENS];
   int   token_count;
   {
   int   i;
   char *temp_str;

   for (i=0; i<token_count; i=i+1) {
      temp_str = token[i];
      while (*temp_str != '\0') {
         if (isupper(*temp_str))
            *temp_str = (char) tolower(*temp_str);
         temp_str++;
         }
      }  /* end for() */
   }  /* end lcase_string() */


INT32
Mini_go_forever()
{
  static  int	complete=0;

   /* Terminal control initialization. */
   io_config.io_control = TERM_USER;	/* 3.1-7 */
   io_config.target_running = TRUE;

#ifndef	MSDOS
   ioctl (fileno(stdin), TCGETA, &OldTermbuf);	/* Initial settings */
#endif

   /* Install ctrl-C handler */

   if (signal (SIGINT, Mini_Ctrl_C_Handler) == SIG_ERR) {
     fprintf(stderr, "Ctrl-C handler not installed.\n"); /* warning */
     if (io_config.echo_mode == (INT32) TRUE)
        fprintf(io_config.echo_file, "Ctrl-C handler not installed.\n"); /* warning */
   }
   /*
   ** Open cmd file (if necessary)
   */

   if (io_config.cmd_file_io == TRUE) {  /* TRUE if -c option given */
      io_config.cmd_file = fopen(io_config.cmd_filename, "r");
        if (io_config.cmd_file == NULL) {
            warning (EMCMDOPEN);
            io_config.cmd_file_io = FALSE;
        } else {
	  /* MON_STDIN is command file */
	  MON_STDIN = fileno(io_config.cmd_file); /* set MON_STDIN */
	};
   }

   Mini_go();  /* set target running */

   do {

      /*
      ** If the target was set to run, get its current status.
      */
      if (Mini_get_target_stats((INT32) udi_waittime, &ProcessorState) != SUCCESS) {
         Mini_TIP_DestroyProc();
         Mini_TIP_exit();
	 fatal_error(EMFATAL);
      }
      GrossState = (int) (ProcessorState & 0xFF);
      switch (GrossState) {
	case	NOTEXECUTING:  /* do nothing */
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		break;
	case	EXITED:  /* do nothing */
		 if (!QuietMode) {
		 fprintf (stderr, "Process exited with 0x%lx\n",
						   (ProcessorState >> 8));
     		 if (io_config.echo_mode == (INT32) TRUE)
		    fprintf (io_config.echo_file, "Process exited with 0x%lx\n",
						   (ProcessorState >> 8));
		 }
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		complete=1;
		break;
	case	RUNNING:	/* any request from target? */
		break;
	case	STOPPED:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		fprintf(stderr, "Execution stopped at ");
     		if (io_config.echo_mode == (INT32) TRUE)
		   fprintf(io_config.echo_file, "Execution stopped at ");
		display_msg();
		break;
	case	BREAK:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		fprintf(stderr, "Breakpoint hit at ");
     		if (io_config.echo_mode == (INT32) TRUE)
		   fprintf(io_config.echo_file, "Breakpoint hit at ");
		display_msg();
		break;
	case	STEPPED:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		fprintf(stderr, "Stepping...Execution stopped at ");
     		if (io_config.echo_mode == (INT32) TRUE)
		   fprintf(io_config.echo_file, "Stepping...Execution stopped at ");
		display_msg();
		break;
	case	WAITING:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		break;
	case	HALTED:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		fprintf(stderr, "Execution halted at ");
     		if (io_config.echo_mode == (INT32) TRUE)
		   fprintf(io_config.echo_file, "Execution halted at ");
		display_msg();
		break;
	case	WARNED:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		break;
	case	TRAPPED:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		PrintTrapMsg((int) (ProcessorState >> 8));
		display_msg();
		break;
	case	STDOUT_READY:
		io_bufsize = 0;
		io_count_done = (INT32) 0;
		do {
		  Mini_get_stdout(io_buffer, IO_BUFSIZE, &io_count_done);
		  write(MON_STDOUT, &io_buffer[0], (int) io_count_done);
		  if (io_config.echo_mode == (INT32) TRUE) {
		     fflush (io_config.echo_file);
		     write (fileno(io_config.echo_file), &io_buffer[0], (int) io_count_done);
		  }
		} while (io_count_done == (INT32) IO_BUFSIZE);
		break;
	case	STDERR_READY:
		io_bufsize = 0;
		io_count_done = (INT32) 0;
		do {
		  Mini_get_stderr(io_buffer, IO_BUFSIZE, &io_count_done);
		  write(MON_STDERR, &io_buffer[0], (int) io_count_done);
		  if (io_config.echo_mode == (INT32) TRUE) {
		     fflush (io_config.echo_file);
		     write (fileno(io_config.echo_file), &io_buffer[0], (int) io_count_done);
		  }
		} while (io_count_done == (INT32) IO_BUFSIZE);
		break;
	case	STDIN_NEEDED:
		/* Line buffered reads only */
		if (io_config.cmd_file_io == TRUE) { /* read from command file */
		   if (Mini_cmdfile_input(io_buffer, IO_BUFSIZE) == SUCCESS) {
		      io_bufsize = strlen(io_buffer);
		      fprintf(stderr, "%s", io_buffer); /* echo */
     		      if (io_config.echo_mode == (INT32) TRUE)
		          fprintf(io_config.echo_file, "%s", io_buffer); /* echo */
		   } else { /* read from terminal */
		     io_bufsize = read( fileno(stdin), io_buffer, IO_BUFSIZE );
		   }
		} else {
		   io_bufsize = read( fileno(stdin), io_buffer, IO_BUFSIZE );
		};
		if (io_bufsize < 0)
		{
		   fprintf(stderr, "fatal error reading from stdin\n");
     		   if (io_config.echo_mode == (INT32) TRUE)
		       fprintf(io_config.echo_file, "fatal error reading from stdin\n");
		}
		if (io_config.echo_mode == (INT32) TRUE) {
		  fflush (io_config.echo_file);
		  write (fileno(io_config.echo_file), &io_buffer[0], (int) io_bufsize);
		}
		Mini_put_stdin(io_buffer, io_bufsize, &io_count_done);
		break;
	case	STDINMODEX:
		/* call TIP to get StdinMode */
		Mini_stdin_mode_x((INT32 *)&TipStdinMode);
		if (TipStdinMode & TIP_NBLOCK)
		  io_config.io_control = TERM_29K;
		else if (TipStdinMode & TIP_ASYNC)
		  io_config.io_control = TERM_29K;
		else if (TipStdinMode == TIP_COOKED)
		  io_config.io_control = TERM_USER;
		else {
		  fprintf(stderr, "DFEWARNING: TIP Requested Stdin Mode Not Supported.\n");
		  fprintf(stderr, "DFEWARNING: Using default mode.\n");
		  TipStdinMode = TIP_COOKED;
		  io_config.io_control = TERM_USER;
		}
		if (io_config.io_control == TERM_29K)
		  display_term29k();
		break;

	default:
		complete=1;
		io_config.io_control = TERM_USER;
                io_config.target_running = FALSE;
		break;
      }; /* end switch */
#ifdef	MSDOS
      if (!complete)
	kbhit();	/* Poll for Ctrl-C */
#endif
      if (CtrlCHit) {
	 CtrlCHit = 0;
	 complete = 1;
      }

      if (io_config.io_control == TERM_29K)
	if (GrossState == RUNNING)
          Mini_poll_channel0();	/* non-blocking */
      else
	TipStdinMode = TIP_COOKED;

   } while (!complete);
#ifndef MSDOS
   ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif

   fflush(stdout);
   fflush(stderr);

   Mini_TIP_DestroyProc();
   Mini_TIP_exit();

   NumberOfConnections=0;
   return (SUCCESS);
}


INT32
get_pc_addrs(pc1, cps)
ADDR32	*pc1;
ADDR32	*cps;
{
   ADDR32	pc_1;
   ADDR32	cps_b;
   INT32	hostendian;
   INT32	bytes_ret;
   INT32	retval;

   hostendian = FALSE;
   if ((retval = Mini_read_req (PC_SPACE,
				(ADDR32) 0, /* doesn't matter */
				(INT32) 1,
				(INT16) 4, /* size */
				&bytes_ret,
				(BYTE *) &pc_1,
				hostendian)) != SUCCESS) {
	return(FAILURE);
   };

   *pc1 = (ADDR32) pc_1;
   if (host_config.host_endian != host_config.target_endian)  {
      convert32((BYTE *)pc1);
   }

   /* get cps */
   hostendian = FALSE;
   if ((retval = Mini_read_req (SPECIAL_REG,
				(ADDR32) 2,
				(INT32) 1,
				(INT16) 4, /* size */
				&bytes_ret,
				(BYTE *) &cps_b,
				hostendian)) != SUCCESS) {
	return(FAILURE);
   };
   *cps = (ADDR32) cps_b;
   if (host_config.host_endian != host_config.target_endian)  {
      convert32((BYTE *)cps);
   }

   return (SUCCESS);
}

INT32
get_pc1_inst(cps, pc1, inst)
ADDR32	cps;
ADDR32	pc1;
BYTE	*inst;
{
   INT32	bytes_ret;
   INT32	hostendian;
   INT32	retval;
   INT32	memory_space;

   hostendian = FALSE;

   if (cps & 0x100L) /* RE bit */
     memory_space = I_ROM;
   else
     memory_space = I_MEM;

   if ((retval = Mini_read_req(memory_space,
			       pc1,
			       (INT32) 1,
			       (INT16) sizeof(INST32),  /* size */
			       &bytes_ret,
			       (BYTE *) inst,
			       hostendian)) != SUCCESS) {
	return(FAILURE);
   };
   return (SUCCESS);
}

void
display_msg()
{
		ADDR32	c_pc1;
		ADDR32	c_cps;

		union instruction_t {
		  BYTE	buf[sizeof(struct instr_t)];
		  struct instr_t  instr;
		};
		union instruction_t instruction;
		struct	instr_t		temp;

		(void) get_pc_addrs(&c_pc1, &c_cps);
		(void) get_pc1_inst(c_cps, c_pc1, instruction.buf);
		fprintf(stderr, " %08lx\n", c_pc1);
     		if (io_config.echo_mode == (INT32) TRUE)
		    fprintf(io_config.echo_file, " %08lx\n", c_pc1);
		if (host_config.target_endian == LITTLE) {
		  temp.op = instruction.instr.b;
		  temp.c = instruction.instr.a;
		  temp.a = instruction.instr.c;
		  temp.b = instruction.instr.op;
		} else { /* default BIG endian */
		  temp.op = instruction.instr.op;
		  temp.c = instruction.instr.c;
		  temp.a = instruction.instr.a;
		  temp.b = instruction.instr.b;
		}
		fprintf(stderr, "%08lx\t %02x%02x%02x%02x\t", c_pc1,
				     temp.op,
				     temp.c,
				     temp.a,
				     temp.b);
     		if (io_config.echo_mode == (INT32) TRUE)
		   fprintf(io_config.echo_file, "%08lx\t %02x%02x%02x%02x\t", c_pc1,
				     temp.op,
				     temp.c,
				     temp.a,
				     temp.b);
		(void) dasm_instr(c_pc1, &(temp));
	  if (io_config.io_control == TERM_USER) {
   	    fprintf(stderr, "\n%s>", ProgramName);
	    fflush(stderr);
     	    if (io_config.echo_mode == (INT32) TRUE)
   		   fprintf(io_config.echo_file, "\n%s>", ProgramName);
	  }
}

int
Mini_cmdfile_input(cmd_buffer, size)
char	*cmd_buffer;
int	size;
{
 if (fgets(cmd_buffer, size, io_config.cmd_file) == NULL) {
   io_config.cmd_file_io = FALSE;
   (void) fclose(io_config.cmd_file);
   MON_STDIN = fileno (stdin); /* reset to terminal after EOF */
   return (FAILURE);
 } else {
   return (SUCCESS);
 }
}

void
Mini_Ctrl_C_Handler(num)
int	num;
{
   CtrlCHit = 1;  /* used for run-only mode, no debugging */
   if (io_config.io_control == TERM_29K) {
#ifndef	MSDOS
     ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
   }
   if (io_config.target_running == TRUE)
      Mini_break();
   io_config.cmd_ready == FALSE;
#ifdef MSDOS
  if (signal (SIGINT, Mini_Ctrl_C_Handler) == SIG_ERR) {
     fprintf(stderr, "Ctrl-C handler not installed.\n"); /* warning */
     if (io_config.echo_mode == (INT32) TRUE)
        fprintf(io_config.echo_file, "Ctrl-C handler not installed.\n"); /* warning */
  }
#endif
  return;
}

static	int
FindCmdIndx(CmdString)
char	*CmdString;
{
  int	i;

  i = 0;
  while (MonitorCommands[i].CmdString) {
    if (strcmp(CmdString, MonitorCommands[i].CmdString))
      i++;
    else
      return (i);
  };
  return (-1);
}

INT32
escape_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  int	retval;
#ifdef	MSDOS
  if ((retval = system ((char *) getenv("COMSPEC"))) != 0)
     return ((INT32) EMDOSERR);
  return ((INT32) SUCCESS);
#else
  if ((retval = system ((char *) getenv("SHELL"))) != 0)
    return ((INT32) EMSYSERR);
  return ((INT32) SUCCESS);
#endif
}

INT32
dummy_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  return ((INT32) 0);
}

INT32
quit_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
   int	i;

   for (i =0; i < NumberOfConnections; i++) {
      Mini_TIP_SetCurrSession(Session_ids[i]);
      Mini_TIP_DestroyProc();
      Mini_TIP_exit();
   };
   fflush(stdout);
   fflush(stderr);
   exit_loop = TRUE;
   NumberOfConnections=0;
   return ((INT32) 0);
}

INT32
connect_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;

  if (tokencnt < 2)
    return (EMSYNTAX);

  if ((retval = Mini_TIP_init(token[1], &Session_ids[NumberOfConnections]))
				       == SUCCESS) {
      NumberOfConnections=NumberOfConnections+1;
  };

  return ((INT32) retval);
}

INT32
disconnect_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;
  int		i;

  if ((retval = Mini_TIP_disc()) != SUCCESS)
     return ((INT32) retval);
  else { /* find some other session */
     NumberOfConnections=NumberOfConnections - 1;
     for (i = 0; i < NumberOfConnections; i++) {
        if ((retval = Mini_TIP_SetCurrSession(Session_ids[i])) == SUCCESS) 
	    return (retval);
     }
     if (i >= NumberOfConnections)  { /* exit DFE */
	 exit_loop = TRUE;
     }
  }

  return ((INT32) retval);
}

INT32
create_proc_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;

  retval = Mini_TIP_CreateProc();

  return ((INT32) retval);
}

INT32
capab_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;

  retval = Mini_TIP_Capabilities();

  return ((INT32) retval);
}

INT32
exit_conn_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;
  int		i;

  if ((retval = Mini_TIP_exit()) != SUCCESS) {;
     return (retval);
  } else { /* find some other session */
     NumberOfConnections=NumberOfConnections - 1;
     for (i = 0; i < NumberOfConnections; i++) {
        if ((retval = Mini_TIP_SetCurrSession(Session_ids[i])) == SUCCESS) 
	    return (retval);
     }
     if (i >= NumberOfConnections)  { /* exit DFE */
	 exit_loop = TRUE;
     }
  }


  return ((INT32) retval);
}

INT32
init_proc_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;

  retval = Mini_send_init_info(&init_info);

  return ((INT32) retval);
}

INT32
destroy_proc_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;

  retval = Mini_TIP_DestroyProc();

  return ((INT32) retval);
}

INT32
set_sessionid_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;
  int		sid;

  if (tokencnt < 2)
    return (EMSYNTAX);

  if (sscanf(token[1],"%d",&sid) != 1)
    return (EMSYNTAX);

  retval = Mini_TIP_SetCurrSession(sid);

  return ((INT32) retval);
}

INT32
set_pid_cmd(token, tokencnt)
char	**token;
int	tokencnt;
{
  INT32		retval;
  int		pid;

  if (tokencnt < 2)
    return (EMSYNTAX);

  if (sscanf(token[1],"%d",&pid) != 1)
    return (EMSYNTAX);

  retval = Mini_TIP_SetPID(pid);

  return ((INT32) retval);
}


INT32
go_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {

   INT32	retval;

   if ((retval = Mini_go()) != SUCCESS) {
     return(FAILURE);
   } else {
     GoCmdFlag = 1;
     BlockMode = NONBLOCK;
     if (TipStdinMode & TIP_NBLOCK) 
	  io_config.io_control = TERM_29K;
     else if (TipStdinMode & TIP_ASYNC)
	  io_config.io_control = TERM_29K;
     else if (TipStdinMode == TIP_COOKED)
	  io_config.io_control = TERM_USER;
     else {
          TipStdinMode = TIP_COOKED;
	  io_config.io_control = TERM_USER;
     }
     io_config.target_running = TRUE;
     return(SUCCESS);
   };

}  /* end go_cmd() */

/*
** This command is used to "trace" or step through code.
** A "t" command with no parameters defaults to a single.
** step.  A "t" command with an integer value following
** steps for as many instructions as is specified by
** that integer.
*/

INT32
trace_cmd(token, token_count)
   char   *token[];
   int     token_count;
   {
   int    result;
   INT32  count;
   INT32	retval;

   if (token_count == 1) {
      count = 1; 
      }
   else
   if (token_count >= 2) {
      result = get_word(token[1], &count);
      if (result != 0)
         return (EMSYNTAX);
      }

   if ((retval = Mini_step(count)) != SUCCESS) {
     return(FAILURE);
   } else {
     GoCmdFlag = 1;
     BlockMode = NONBLOCK;
     if (TipStdinMode & TIP_NBLOCK) 
	  io_config.io_control = TERM_29K;
     else if (TipStdinMode & TIP_ASYNC)
	  io_config.io_control = TERM_29K;
     else if (TipStdinMode == TIP_COOKED)
	  io_config.io_control = TERM_USER;
     else {
          TipStdinMode = TIP_COOKED;
	  io_config.io_control = TERM_USER;
     }
     io_config.target_running = TRUE;
     return(SUCCESS);
   }

   }  /* end trace_cmd() */

/*
 * The "ch0" command is used to send characters (input) to the application
 * program asynchronously. This command deinstalls the control-C handler,
 * sets up input to raw mode, polls the keyboard, sends the bytes to the
 * TIP. The command is exited when Ctrl-U is typed.
 */

INT32
channel0_cmd(token, token_count)
   char   *token[];
   int     token_count;
{
      io_config.io_control = TERM_29K;
#ifndef MSDOS
      ioctl (fileno(stdin), TCGETA, &NewTermbuf); 	/* New settings */
      NewTermbuf.c_lflag &= ~(ICANON);
      NewTermbuf.c_cc[4] = 0;		/* MIN */
      NewTermbuf.c_cc[5] = 0;		/* TIME */
      ioctl (fileno(stdin), TCSETA, &NewTermbuf); /* Set new settings */
#endif
  return (0);
}

/*
 * Only for stdin, not for command file input 
 */
INT32
Mini_poll_channel0()
{
  BYTE	ch;

    /* read from terminal */
#ifdef MSDOS
      /* CBREAK mode */
      if (kbhit()) {
         ch = (unsigned char) getche();
	 if (io_config.echo_mode == (INT32) TRUE) {
	   putc (ch, io_config.echo_file);
	   fflush (io_config.echo_file);
	 }
         if (ch == (BYTE) TOGGLE_CHAR) { /* Ctrl-U typed, give control back to User */
           io_config.io_control = TERM_USER;
	   display_termuser();
           return (0);
         } else {
	   if (ch == (unsigned char) 13) { /* \r, insert \n */
	     putchar(10);	/* line feed */
	     if (io_config.echo_mode == (INT32) TRUE) {
	       putc (ch, io_config.echo_file);
	       fflush (io_config.echo_file);
	     }
	   }
#ifdef MSDOS
	   if (ch == (unsigned char) 10) { /* \n, ignore \n */
	     return (0);
	   }
#endif
           Mini_put_stdin((char *)&ch, 1, &io_count_done);
           return (0);
         }
       }
       return(0);
#else	/* Unix */
     /* 
      * Set STDIN to CBREAK mode. For each character read() send it
      * to TIP using Mini_put_stdin(). This is done only if the
      * terminal is controlled by the 29K Target System, i.e. when
      * io_config.io_control == TERM_29K. Otherwise, this function should
      * not be called as it would affect the command-line processing.
      */
      /* while ((io_bufsize = read (fileno(stdin), &ch, 1)) == 1) { */
      if ((io_bufsize = read (fileno(stdin), &ch, 1)) == 1) { 
	if (io_config.echo_mode == (INT32) TRUE) {
	  putc (ch, io_config.echo_file);
	  fflush (io_config.echo_file);
	}
	if (ch == (BYTE) TOGGLE_CHAR) { /* process ctrl-U */
         ioctl (fileno(stdin), TCSETA, &OldTermbuf); /* reset old settings */
         io_config.io_control = TERM_USER;
	 display_termuser();
         return (0);
	} else { /* send it to TIP */
         Mini_put_stdin((char *)&ch, 1, &io_count_done);
	}
      }
     return (0);
#endif
} /* end Mini_poll_channel0() */

void
PrintTrapMsg(num)
int	num;
{
  if ((num >= 0) && (num <= 22)) {
    fprintf(stderr, "%s Trap occurred at ", TrapMsg[num]);
    if (io_config.echo_mode == (INT32) TRUE)
       fprintf(io_config.echo_file, "%s Trap occurred at ", TrapMsg[num]);
  } else {
    fprintf(stderr, "Trap %d occurred at ");
    if (io_config.echo_mode == (INT32) TRUE)
       fprintf(io_config.echo_file, "Trap %d occurred at ");
  }
}

void
display_term29k()
{
    fprintf(stderr,"\nTerminal controlled 29K target...Type Ctrl-U <ret> for mondfe prompt\n");
    fflush (stderr);
    if (io_config.echo_mode == (INT32) TRUE)
       fprintf(stderr,"\nTerminal controlled 29K target...Type Ctrl-U <ret> for mondfe prompt\n");
#ifndef MSDOS
      ioctl (fileno(stdin), TCGETA, &NewTermbuf); 	/* New settings */
      NewTermbuf.c_lflag &= ~(ICANON);
      NewTermbuf.c_cc[4] = 0;		/* MIN */
      NewTermbuf.c_cc[5] = 0;		/* TIME */
      ioctl (fileno(stdin), TCSETA, &NewTermbuf); /* Set new settings */
#endif
}

void
display_termuser()
{
#ifndef MSDOS
    ioctl (fileno(stdin), TCSETA, &OldTermbuf); /*reset settings */
#endif
   /* Print a prompt */
  fprintf(stderr, "\n%s>", ProgramName);
  if (io_config.echo_mode == (INT32) TRUE)
    fprintf(io_config.echo_file, "\n%s>", ProgramName);
}

INT32
quietmode_off(token, token_count)
   char   *token[];
   int     token_count;
{
  QuietMode = 0;
  return (0);
}

INT32
quietmode_on(token, token_count)
   char   *token[];
   int     token_count;
{
  QuietMode = 1;
  return (0);
}

INT32
logoff_cmd(token, token_count)
   char   *token[];
   int     token_count;
{
   if (io_config.log_mode == (INT32) TRUE) { 
     io_config.log_mode = (INT32) FALSE;
     (void) fclose(io_config.log_file);
   }
   return (0);
}

INT32
logon_cmd(token, token_count)
   char   *token[];
   int     token_count;
{
   if (io_config.log_mode == (INT32) FALSE) { 
     if (strcmp(io_config.log_filename, "\0") != 0) {/* valid file */
       io_config.log_mode = (INT32) TRUE;
       if ((io_config.log_file = fopen(io_config.log_filename, "a")) == NULL)
       {
          io_config.log_mode = (INT32) FALSE;
          warning(EMLOGOPEN);
       };
     } else {
       warning(EMLOGOPEN);
     }
   }
   return (0);
}

INT32
set_logfile(token, token_count)
   char   *token[];
   int     token_count;
{
  if (token_count < 2) /* insufficient number of args */
    return (EMSYNTAX);

  (void) strcpy ((char *)(&(io_config.log_filename[0])),token[1]);

  if (io_config.log_mode == (INT32) TRUE) { /* replace log file used */
	 if ((io_config.log_file = 
		      fopen (io_config.log_filename, "w")) == NULL) {
	    warning (EMLOGOPEN);
	    io_config.log_mode = (INT32) FALSE;
	 }
  } else {
	 io_config.log_mode = (INT32) TRUE;
	 if ((io_config.log_file = 
		      fopen (io_config.log_filename, "w")) == NULL) {
	    warning (EMLOGOPEN);
	    io_config.log_mode = (INT32) FALSE;
	 } 
  }
  return (0);

}

INT32
echomode_on(token, token_count)
   char   *token[];
   int     token_count;
{
  if (io_config.echo_mode == (INT32) FALSE) {
    if (strcmp(io_config.echo_filename, "\0") != 0) { /* if valid file in effect */
       io_config.echo_mode = (INT32) TRUE;
       if ((io_config.echo_file = fopen (io_config.echo_filename, "a")) == NULL)
       {
	 warning (EMECHOPEN);
	 io_config.echo_mode = (INT32) FALSE;
       }
    } else
       warning(EMINVECHOFILE);
  }
  return (0);
}

INT32
echomode_off(token, token_count)
   char   *token[];
   int     token_count;
{
  if (io_config.echo_mode == (INT32) TRUE)  {
    io_config.echo_mode = (INT32) FALSE;
    (void) fclose(io_config.echo_file);
  }
  return (0);
}

INT32
echofile_cmd(token, token_count)
   char   *token[];
   int     token_count;
{
  if (token_count < 2) /* insufficient number of args */
    return (EMSYNTAX);

  (void) strcpy ((char *)(&(io_config.echo_filename[0])),token[1]);

  if (io_config.echo_mode == (INT32) TRUE) { /* replace echo file used */
	 if ((io_config.echo_file = 
		      fopen (io_config.echo_filename, "w")) == NULL) {
	    warning (EMECHOPEN);
	    io_config.echo_mode = (INT32) FALSE;
	 }
  } else {
	 io_config.echo_mode = (INT32) TRUE;
	 if ((io_config.echo_file = 
		      fopen (io_config.echo_filename, "w")) == NULL) {
	    warning (EMECHOPEN);
	    io_config.echo_mode = (INT32) FALSE;
	 } 
  }
  return (0);
}

INT32
cmdfile_cmd(token, token_count)
   char   *token[];
   int     token_count;
{
  if (token_count < 2)
     return (EMSYNTAX);

  (void) strcpy((char *)(&(io_config.cmd_filename[0])),token[1]);

  if (io_config.cmd_file_io == (INT32) TRUE) {
    warning (EMCMDFILENEST); /* command file nesting not allowed */
  } else {
    io_config.cmd_file_io = (INT32) TRUE;
    if ((io_config.cmd_file = fopen (io_config.cmd_filename,"r")) == NULL) {
      warning (EMCMDOPEN);
      io_config.cmd_file_io = (INT32) FALSE;
    } else {
       /* MON_STDIN is command file */
       MON_STDIN = fileno(io_config.cmd_file); /* set MON_STDIN */
    }
  }

  return (0);
}

INT32
tip_cmd(token, token_count)
   char   *token[];
   int     token_count;
{
  if (token_count < 2)
    return (EMSYNTAX);

  sprintf(tip_cmd_buffer, "%s %s\0", token[0], token[1]);

  Mini_put_trans(tip_cmd_buffer);

  return (0);
}
