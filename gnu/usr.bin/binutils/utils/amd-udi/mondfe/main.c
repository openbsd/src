static char _[] = "@(#)main.c	5.27 93/10/27 15:11:04, Srini, AMD";
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
 * This is the main module of MONDFE.
 *****************************************************************************
 */

#include <stdio.h>
#include  <signal.h>
#ifdef	MSDOS
#include <stdlib.h>
#include <string.h>
#else
#include <strings.h>
#endif
#include "coff.h"
#include "main.h"
#include  "monitor.h"
#include "memspcs.h"
#include "miniint.h"
#include "error.h"
#include "versions.h"

#ifdef MSDOS
#define	strcasecmp	stricmp
#endif

/* Externals */
extern	void	Def_CtrlC_Hdlr PARAMS((int));
extern	void	Mini_parse_args PARAMS((int argc, char  **argv));
extern	INT32	Mini_initialize PARAMS((HOST_CONFIG *host, IO_CONFIG *io,
					INIT_INFO *init));
extern	INT32	Mini_io_setup PARAMS((void));
extern	INT32	Mini_io_reset PARAMS((void));
extern	INT32	Mini_load_file PARAMS((char *fname, INT32 mspace, 
				       int  argc, char  *args, 
				       INT32 symbols, INT32 sections, int msg));
extern	void	Mini_monitor PARAMS((void));
extern	INT32	Mini_go_forever PARAMS((void));

/* Globals */

GLOBAL	char  *host_version = HOST_VERSION;
GLOBAL	char  *host_date = HOST_DATE;

GLOBAL 	TARGET_CONFIG 	target_config;
GLOBAL 	VERSIONS_ETC 	versions_etc;
GLOBAL 	TARGET_STATUS 	target_status;
GLOBAL 	HOST_CONFIG 	host_config;
GLOBAL 	IO_CONFIG    	io_config;

/* The filenos of the monitor's stdin, stdout, adn stderr */
int	MON_STDIN;
int	MON_STDOUT;
int	MON_STDERR;

int	Session_ids[MAX_SESSIONS];
int	NumberOfConnections=0;
/* The following variables are to be set/initialized in Mini_parse_args()
 */
GLOBAL	BOOLEAN	monitor_enable = FALSE;
GLOBAL	int	QuietMode = 0;
GLOBAL	BOOLEAN	ROM_flag = FALSE;

GLOBAL 	char  	*ROM_file = NULL;
GLOBAL	char	**ROM_argv;
GLOBAL	int	ROM_sym, ROM_sects;
GLOBAL	int	ROM_argc;

GLOBAL	char	CoffFileName[1024];
static	char	Ex_argstring[1024];
GLOBAL	int	Ex_sym, Ex_sects, Ex_space;
GLOBAL	int	Ex_argc;
static	int	Ex_loaded=0;

GLOBAL	char	*ProgramName=NULL;

GLOBAL	char	*connect_string;

GLOBAL	INT32	udi_waittime;

/* Main routine */

main(argc, argv)
   int   argc;
   char *argv[];
   {

   char		*temp;
   int		i;
   UINT32	ProcessorState;
   int		GrossState;
   INT32	retval;

   ProgramName=argv[0];
   if (strpbrk( ProgramName, "/\\" ))
   {
	temp = ProgramName + strlen( ProgramName );
	while (!strchr( "/\\", *--temp ))
		;
	ProgramName = temp+1;
   }

   if (argc < 2 ) {
     fprintf(stderr, "MiniMON29K Release 3.0\n");
     fprintf(stderr, "MONDFE Debugger Front End (UDI 1.2) Version %s Date %s\n", HOST_VERSION, HOST_DATE);
     fatal_error(EMUSAGE);
   }

   /* Initialize stdin, stdout, sdterr to defaults */
   MON_STDIN = fileno(stdin);
   MON_STDOUT = fileno(stdout);
   MON_STDERR = fileno(stderr);
   NumberOfConnections = 0;
   (void) strcpy (CoffFileName,"");

   udi_waittime = (INT32) 10;	/* default poll every ? secs */
   /*
   ** Initialize host configuration structure (global), set defaults
   */
   if (Mini_initialize (&host_config, &io_config, &init_info) != SUCCESS)
      fatal_error(EMHINIT);

   /* Parse args */
   (void) Mini_parse_args(argc, argv);

   if (io_config.echo_mode == (INT32) TRUE) {
      for (i=0; i < argc; i++)
	 fprintf(io_config.echo_file, "%s ", argv[i]);
      fprintf(io_config.echo_file, "\n");
      fflush (io_config.echo_file);
   };

   if ((monitor_enable == FALSE) & !Ex_loaded)
     fatal_error (EMNOFILE);

   /*
   ** Initialize host I/O.
   */

   if (Mini_io_setup() != SUCCESS)
      fatal_error(EMIOSETF);

   /*
   * Initialize TIP. Load ROM file, if necessary.
   ** Open communication channel
   */
   if (signal (SIGINT, Def_CtrlC_Hdlr) == SIG_ERR) {
     fprintf(stderr, "Couldn't install default Ctrl-C handler.\n"); 
     if (io_config.echo_mode == (INT32) TRUE)
        fprintf(io_config.echo_file, "Couldn't install default Ctrl-C handler.\n"); 
   }
   /* connect_string is made by the Mini_parse_args routine */
   retval = Mini_TIP_init(connect_string, &Session_ids[NumberOfConnections]); 
   if (retval > (INT32) 0) {
       fatal_error(EMTIPINIT);
   } else if (retval == (INT32) SUCCESS) {
       NumberOfConnections=NumberOfConnections+1;
   } else {
   	Mini_TIP_exit();
        fatal_error(EMTIPINIT);
   }
   if (Mini_get_target_stats((INT32) -1, &ProcessorState) != SUCCESS) {/* reconnect?*/
         Mini_TIP_exit();
	 fatal_error(EMFATAL);
   };
   GrossState = (int) (ProcessorState & 0xFF);
   if (GrossState == NOTEXECUTING)
      if (Mini_TIP_CreateProc() != SUCCESS) {
         Mini_TIP_exit();
         fatal_error(EMNOPROCESS);
      }

   /* Get capabilities */
   if (Mini_TIP_Capabilities() != SUCCESS) {
      Mini_TIP_DestroyProc();
      Mini_TIP_exit();
      fatal_error(EMNOTCOMP);
   }

   /* Get the target memory configuration, and processor type */
   versions_etc.version = 0;	/* initialize in case not returned */
   if (Mini_config_req(&target_config, &versions_etc) != SUCCESS) {
      warning(EMCONFIG);
   }

   if (strcmp(CoffFileName,"") != 0) {
     if (Mini_load_file(&CoffFileName[0], 
			Ex_space,
			Ex_argc,
			Ex_argstring,
			Ex_sym,
			Ex_sects,
			QuietMode) != SUCCESS) {
	Ex_loaded = 0;
	warning(EMLOADF);
     } else {
        Ex_loaded = 1;
     };
   }

   if (monitor_enable == FALSE) {
      if (Ex_loaded)
         Mini_go_forever();
      else { /* nothing to do, so quit */
         Mini_TIP_DestroyProc();
         Mini_TIP_exit();
	 warning (EMNOFILE);
      }
   } else {
      Mini_monitor();
   };

   fflush(stderr);
   fflush(stdout);

   /* Perform host-specific clean-up */
   if (Mini_io_reset() != SUCCESS)
      warning(EMIORESETF);

   if (!QuietMode) {
      fprintf(stderr, "\nGoodbye.\n");
      if (io_config.echo_mode == (INT32) TRUE) {
          fprintf(io_config.echo_file, "\nGoodbye.\n");
          (void) fclose (io_config.echo_file);
      }
   }
   return(0);

   }   /* end Main */



/*
** Functions
*/

/*
** This function prints out a fatal error message 
** from error_msg[]. 
** Finally, the program exits with error_number.
*/
#ifndef	MINIMON
extern	UINT32	UDIGetDFEIPCId PARAMS((void));
#endif

void
fatal_error(error_number)
   INT32 error_number;
   {
   UINT32	IPCId;

   if (error_number == (INT32) EMUSAGE) {
#ifndef	MINIMON
     IPCId = (UINT32) UDIGetDFEIPCId();
     fprintf(stderr, "MONDFE UDI IPC Implementation Id %d.%d.%d\n",
				(int) ((IPCId & 0xf00) >> 8),
				(int) ((IPCId & 0xf0) >> 4),
				(int) (IPCId & 0xf));
     if (io_config.echo_mode == (INT32) TRUE)
       fprintf(io_config.echo_file, "MONDFE UDI IPC Implementation Id %d.%d.%d\n",
				(int) ((IPCId & 0xf00) >> 8),
				(int) ((IPCId & 0xf0) >> 4),
				(int) (IPCId & 0xf));
#else
     fprintf(stderr, "Procedurally linked MiniMON29K 3.0 Delta\n");
#endif
       fprintf(stderr, "Usage: %s %s\nGoodbye.\n", 
				  ProgramName, error_msg[(int)error_number]);
       if (io_config.echo_mode == (INT32) TRUE)
          fprintf(io_config.echo_file, "Usage: %s %s\nGoodbye.\n", 
				  ProgramName, error_msg[(int)error_number]);
   } else {
     fprintf(stderr, "DFEERROR: %d : %s\nFatal error. Exiting.\n", 
			(int)error_number, error_msg[(int)error_number]);
     if (io_config.echo_mode == (INT32) TRUE)
        fprintf(io_config.echo_file, "DFEERROR: %d : %s\nFatal error. Exiting.\n", 
			(int)error_number, error_msg[(int)error_number]);
   }

   NumberOfConnections=0;
   if (io_config.echo_mode == (INT32) TRUE)
     (void) fclose(io_config.echo_file);

   exit((int) error_number);
   }


/*
** This function prints out a warning message from
** the error_msg[] string array.
*/

void
warning(error_number)
   INT32 error_number;
   {
   fprintf(stderr, "DFEWARNING: %d : %s\n", (int) error_number, error_msg[(int)error_number]);
   if (io_config.echo_mode == (INT32) TRUE)
      fprintf(io_config.echo_file, "DFEWARNING: %d : %s\n", error_number, error_msg[(int)error_number]);
   }


/* Parse the command line arguments */
void
Mini_parse_args(argc, argv)
int	argc;
char	**argv;
{
   int		i, j;
   int		len;

   len = 0;
   for (i = 1; i < argc; i++)  {	/* ISS */
      len = len + (int) strlen(argv[i]);
   };
   if (len == (int) 0) {
     connect_string = NULL;
   } else {
     if ((connect_string = (char *) malloc (len + argc)) == NULL) {
	fatal_error(EMALLOC);
     };
     for (i = 1; i < argc; i++)  {	/* ISS */
      if (strcasecmp(argv[i], "-TIP") == 0) {
	  i++;
          if (i >= argc)
            fatal_error(EMUSAGE);
	  connect_string = argv[i];
      } else if (strcmp(argv[i], "-log") == 0) {
	i++;
	if (i >= argc)
	  fatal_error(EMUSAGE);
	(void) strcpy((char *)(&(io_config.log_filename[0])),argv[i]);
	io_config.log_mode = (INT32) TRUE;
      } else if (strcmp (argv[i], "-w") == 0) { /* Wait time param */
	i++;
	if (i >= argc)
	  fatal_error(EMUSAGE);
	if (sscanf(argv[i], "%ld", &udi_waittime) != 1)
	  fatal_error(EMUSAGE);
      } else if (strcmp (argv[i], "-ms") == 0) { /* mem stack size */
	i++;
	if (i >= argc)
	  fatal_error(EMUSAGE);
	if (sscanf(argv[i], "%lx", &init_info.mem_stack_size) != 1)
	  fatal_error(EMUSAGE);
      } else if (strcmp (argv[i], "-rs") == 0) { /* reg stack size */
	i++;
	if (i >= argc)
	  fatal_error(EMUSAGE);
	if (sscanf(argv[i], "%lx", &init_info.reg_stack_size) != 1)
	  fatal_error(EMUSAGE);
      } else if (strcmp(argv[i], "-d") == 0) {	
         monitor_enable = TRUE;	
      } else if (strcasecmp(argv[i], "-D") == 0) {	
         monitor_enable = TRUE;	
      } else if (strcmp(argv[i], "-le") == 0) {	
         host_config.target_endian = LITTLE;	
      } else if (strcmp(argv[i], "-q") == 0) {	
         QuietMode = 1;	
      } else if (strcmp(argv[i], "-c") == 0) {
         i++;
         if (i >= argc)
            fatal_error(EMUSAGE);
         (void) strcpy((char *)(&(io_config.cmd_filename[0])),argv[i]);
         io_config.cmd_file_io = TRUE;
      } else if (strcmp(argv[i], "-e") == 0) {
         i++;
         if (i >= argc)
            fatal_error(EMUSAGE);
         (void) strcpy((char *)(&(io_config.echo_filename[0])),argv[i]);
	 io_config.echo_mode = (INT32) TRUE;
	 if ((io_config.echo_file = fopen (io_config.echo_filename, "w")) == NULL) {
	    warning (EMECHOPEN);
	    io_config.echo_mode = (INT32) FALSE;
	 }
      } else { 
	 (void) strcpy (&CoffFileName[0], argv[i]);
	 Ex_argc = argc - i;
	 (void) strcpy(Ex_argstring, argv[i]);
	 for (j=1; j < Ex_argc; j++) {
	   (void) strcat(Ex_argstring, " ");
	   (void) strcat (Ex_argstring, argv[i+j]);
	 }
	 Ex_sym = 0;
	 Ex_sects = (STYP_ABS|STYP_TEXT|STYP_LIT|STYP_DATA|STYP_BSS);
	 Ex_space = (INT32) I_MEM;
	 Ex_loaded = 1;  /* given */
	 break;
       }  
     }; /* end for */
   }; /* end if-else */
}

/* Function to initialize host_config and io_config data structures
 * to their default values *
 */

INT32
Mini_initialize(host, io, init)
HOST_CONFIG	*host;
IO_CONFIG	*io;
INIT_INFO	*init;
{
   /* Initialize host configuration information */

#ifdef	MSDOS
   host->host_endian = LITTLE;
#else
   host->host_endian = BIG;
#endif

   host->target_endian = BIG;  /* default */
   host->version = host_version;
   host->date = host_date;

   /* Initialize I/O configuration information */

   io->hif = TRUE;
   io->io_control = TERM_USER;
   io->cmd_ready = FALSE;
   io->clear_to_send = TRUE;
   io->target_running = FALSE;
   io->cmd_file = NULL;
   io->cmd_filename[0] = '\0';
   io->cmd_file_io = FALSE;
   io->log_mode = FALSE;
   io->log_file = NULL;
   io->log_filename[0] = '\0';
   io->echo_mode = FALSE;
   io->echo_file = NULL;
   io->echo_filename[0] = '\0';
   io->io_toggle_char = (BYTE) 21;  /* CTRL-U */

   init->mem_stack_size = (UINT32) -1;
   init->reg_stack_size = (UINT32) -1;
   return(SUCCESS);
}

void
Def_CtrlC_Hdlr(num)
int	num;
{
  Mini_io_reset();
   if (!QuietMode) {
      fprintf(stderr, "\nInterrupted.\n");
      if (io_config.echo_mode == (INT32) TRUE) {
          fprintf(io_config.echo_file, "\nInterrupted.\n");
          (void) fclose (io_config.echo_file);
      }
   }
   Mini_TIP_SetCurrSession(0);
   Mini_TIP_exit();
  exit(1);
}

