/* @(#)monitor.h	5.19 93/08/23 15:31:18, Srini, AMD */
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
 * This header file declares the data structures and functions used by the 
 * monitor.c module of DFE.
 *****************************************************************************
 */

/* Data structures that don't get used unless the monitor is
 * invoked.
 */

#ifndef	_MONITOR_H_INCLUDED_
#define	_MONITOR_H_INCLUDED_

#include  "types.h"

/* Monitor command limitations */

#define MAX_TOKENS          25
#define BUFFER_SIZE        256
#define	MAXFILENAMELEN	   256

/* Define target status: these correspond to UDI defined defined */
#define	TRAPPED		0
#define	NOTEXECUTING	1
#define	RUNNING		2
#define	STOPPED		3
#define	WARNED		4
#define	STEPPED		5
#define	WAITING		6
#define	HALTED		7
#define	STDOUT_READY	8
#define	STDERR_READY	9
#define	STDIN_NEEDED	10
#define	STDINMODEX	11
#define	BREAK		12
#define	EXITED		13

/*
** Dump and set routine definitions
*/

#define WORD_FORMAT    0
#define HALF_FORMAT    1
#define BYTE_FORMAT    2
#define FLOAT_FORMAT   3
#define DOUBLE_FORMAT  4

/*
 * Keyboard polling modes.
 */
#define	BLOCK		1
#define	NONBLOCK	0

/*
** Structure for breakpoint array
*/

struct bkpt_t {
   int    break_id;
   INT32    memory_space;
   ADDR32   address;
   INT32    pass_count;
   INT32    curr_count;
   INT32    bkpt_type;
   struct  bkpt_t  *next;
   };

extern	struct	bkpt_t	*bkpt_table;
extern	INT32	udi_waittime;

/* Monitor command functions */

INT32   asm_cmd PARAMS((char **, int));
INT32   bkpt_cmd PARAMS((char **, int));
INT32   config_cmd PARAMS((char **, int));
INT32   cmdfile_cmd PARAMS((char **, int));
INT32   dasm_cmd PARAMS((char **, int));
INT32   dump_cmd PARAMS((char **, int));
INT32   echomode_on PARAMS((char **, int));
INT32   echomode_off PARAMS((char **, int));
INT32   echofile_cmd PARAMS((char **, int));
INT32   fill_cmd PARAMS((char **, int));
INT32   go_cmd PARAMS((char **, int));
INT32   help_cmd PARAMS((char **, int));
INT32   io_toggle_cmd PARAMS((char **, int));
INT32   kill_cmd PARAMS((char **, int));
INT32   move_cmd PARAMS((char **, int));
INT32   reset_cmd PARAMS((char **, int));
INT32   set_cmd PARAMS((char **, int));
INT32   trace_cmd PARAMS((char **, int));
INT32   channel0_cmd PARAMS((char **, int));
INT32   Mini_poll_channel0 PARAMS((void));
INT32   version_cmd PARAMS((char **, int));
INT32   x_cmd PARAMS((char **, int));
INT32   xp_cmd PARAMS((char **, int));
INT32   xc_cmd PARAMS((char **, int));
INT32   i_cmd PARAMS((char **, int));
INT32   ix_cmd PARAMS((char **, int));
INT32   il_cmd PARAMS((char **, int));
INT32   yank_cmd PARAMS((char **, int));
INT32	quit_cmd PARAMS((char **, int));
INT32	quietmode_on PARAMS((char **, int));
INT32	quietmode_off PARAMS((char **, int));
INT32	dummy_cmd PARAMS((char **, int));
INT32	connect_cmd PARAMS((char **, int));
INT32	disconnect_cmd PARAMS((char **, int));
INT32	create_proc_cmd PARAMS((char **, int));
INT32	exit_conn_cmd PARAMS((char **, int));
INT32	destroy_proc_cmd PARAMS((char **, int));
INT32	set_pid_cmd PARAMS((char **, int));
INT32	capab_cmd PARAMS((char **, int));
INT32	set_sessionid_cmd PARAMS((char **, int));
INT32	init_proc_cmd PARAMS((char **, int));
INT32	escape_cmd PARAMS((char **, int));
INT32	tip_cmd PARAMS((char **, int));
INT32	logon_cmd PARAMS((char **, int));
INT32	logoff_cmd PARAMS((char **, int));
INT32	set_logfile PARAMS((char **, int));

#endif /* _MONITOR_H_INCLUDED_ */
