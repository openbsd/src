/* @(#)miniint.h	5.18 93/07/30 16:40:02, Srini, AMD */
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
 * 29K Systems Engineering
 * Mail Stop 573
 * 5204 E. Ben White Blvd.
 * Austin, TX 78741
 * 800-292-9263
 * 29k-support@AMD.COM
 *****************************************************************************
 * Engineer: Srini Subramanian.
 *****************************************************************************
 * This header file defines the interface routines between the DFE and UDI.
 *****************************************************************************
 */
#ifndef	_MINIINT_H_INCLUDED_
#define	_MINIINT_H_INCLUDED_

/* This file contains the declarations of functions that form
 * Minimon frontend's interface to its back-end.
 * Back-end could be a message system or a procedural interface
 */

/* There is one function for each of the message sent from the
* host to the target.
*/

#include	"types.h"

#define	MONErrorMsgSize		80

#define BKPT_29050       0
#define	BKPT_29050_BTE_0	0
#define	BKPT_29050_BTE_1	1
#define BKPT_29000      -1

#define	MONMaxMemRanges	3    /* Inst, data, Rom */
#define	MONMaxChips	2   /* main cpu & coprocessor */
#define	MONMaxProcessMemRanges	2
#define	MONMaxStacks	2

/* For breakpoint status */
#define	MONBreakNoMore	0x1
#define	MONBreakInvalid	0x2

/*  These are defined in main.h                        */
/*	typedef	struct	target_config_t	TARGET_CONFIG; */
/*	typedef	struct  target_status_t TARGET_STATUS; */

/* This is the function to initialize the Target Interphase Process/
 * System.
 * Input: Pointer to the target's name (as given at the "-t" command
 *	  line flag of Minimon).
 * Output: It returns:
 * SUCCESS: if everything went okay.
 * FAILURE: not okay.
 */

INT32	Mini_TIP_init PARAMS((char *conn_str, int   *sid));

INT32	Mini_TIP_SetCurrSession PARAMS((int  sid));

INT32	Mini_TIP_SetPID PARAMS((int  pid));

INT32	Mini_TIP_DestroyProc PARAMS((void));

INT32	Mini_TIP_Capabilities PARAMS((void));

INT32	Mini_TIP_CreateProc PARAMS((void));

INT32	Mini_TIP_disc PARAMS((void));

INT32	Mini_TIP_exit PARAMS((void));

INT32	Mini_reset_processor PARAMS((void));

INT32	Mini_config_req PARAMS((TARGET_CONFIG  *target_conf, VERSIONS_ETC *vers));

INT32	Mini_status_req PARAMS((TARGET_STATUS *target_stat));

INT32	Mini_read_req PARAMS((INT32 memory_space, 
			      ADDR32 address, 
			      INT32 byte_count,
			      INT16  size,
			      INT32 *count_done, 
			      BYTE *buffer, 
			      BOOLEAN host_endian));

INT32	Mini_write_req PARAMS((INT32 memory_space,
			       ADDR32 address,
			       INT32 byte_count,
			       INT16 size,
			       INT32 *count_done,
			       BYTE *buffer,
			       BOOLEAN host_endian));

INT32	Mini_bkpt_set PARAMS((INT32 memory_space,
			      ADDR32 bkpt_addr,
			      INT32 pass_count,
			      INT32 bkpt_type,
			      int *break_id));

INT32	Mini_bkpt_rm PARAMS((int break_id));

INT32	Mini_bkpt_stat PARAMS((int break_id,
			       ADDR32 *bkpt_addr,
			       INT32 *memory_space,
			       INT32 *pass_count,
			       INT32 *bkpt_type ,
			       INT32 *current_cnt));

INT32 	Mini_copy PARAMS((INT32 source_space,
			  ADDR32 source_addr,
			  INT32 dest_space,
			  ADDR32 dest_addr,
			  INT32 byte_count,
			  INT16 size,
			  INT32 count_done));

INT32	Mini_fill PARAMS((INT32 memory_space,
			  ADDR32 start_addr,
			  INT32 fill_count,
			  INT32 byte_count,
			  BYTE *pattern));

INT32	Mini_init PARAMS((ADDR32 text_start,
			  ADDR32 text_end,
			  ADDR32 data_start,
			  ADDR32 data_end,
			  ADDR32 entry_point,
			  INT32 m_stack,
			  INT32 r_stack,
			  char  *arg_string));

INT32 	Mini_go PARAMS((void));

INT32	Mini_step PARAMS((INT32 count));

INT32	Mini_break PARAMS((void));

INT32	Mini_get_target_stats PARAMS((INT32 maxtime, INT32 *target_status));

INT32	Mini_get_stdout PARAMS((char *buffer,
				INT32 bufsize,
				INT32 *count_done));

INT32	Mini_get_stderr PARAMS((char *buffer,
				INT32 bufsize,
				INT32 *count_done));

INT32	Mini_stdin_mode_x PARAMS((INT32 *mode));

INT32	Mini_put_stdin PARAMS((char *buffer,
			       INT32 bufsize,
			       INT32 *count_done));

INT32	Mini_put_trans PARAMS((char *buffer));

#endif /* _MINIINT_H_INCLUDED_ */
