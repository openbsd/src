/* @(#)main.h	5.19 93/07/30 16:39:56, Srini, AMD */
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
 * This header file declares the structures defined in main.c
 *****************************************************************************
 */

#ifndef	_MAIN_H_INCLUDED_
#define	_MAIN_H_INCLUDED_

#include  "types.h"

#define TRUE                 1
#define FALSE                0

#define MEM_STACK_SIZE 		0x6000
#define REG_STACK_SIZE 		0x2000

#define	MAXFILENAMELEN	   256

/* Define BIG and LITTLE endian */
#define BIG                  0
#define LITTLE               1

#define	MAX_SESSIONS	10


/*
** Structure for host configuration
*/


struct host_config_t {
   INT32  comm_interface;
   INT32  host_endian;
   INT32  target_endian;
   INT32  PC_port_base;
   INT32  PC_mem_seg;
   char  *comm_port;
   char  *baud_rate;
   char  *version;
   char  *date;
   };
typedef	struct	host_config_t 	HOST_CONFIG;
extern	HOST_CONFIG	host_config;

struct io_config_t {
   INT32  hif;
   INT32  io_control;
   INT32  cmd_ready;
   INT32  clear_to_send;
   INT32  target_running;
   INT32  cmd_file_io;
   INT32  log_mode;
   INT32  echo_mode;
   FILE   *cmd_file;
   char	  cmd_filename[MAXFILENAMELEN];
   FILE   *log_file;
   char	  log_filename[MAXFILENAMELEN];
   FILE   *echo_file;
   char   echo_filename[MAXFILENAMELEN];
   BYTE   io_toggle_char;
   };
typedef	struct	io_config_t	IO_CONFIG;
extern	IO_CONFIG	io_config;

struct	init_info_t {
          ADDR32   text_start;
          ADDR32   text_end;
          ADDR32   data_start;
          ADDR32   data_end;
          ADDR32   entry_point;
          UINT32    mem_stack_size;
          UINT32    reg_stack_size;
	  char	   *argstring;
};
typedef	struct	init_info_t	INIT_INFO;
extern	INIT_INFO	init_info;


struct  versions_etc_t {
          INT32    version;
	  INT32	   os_version;  /* os version is returned here */
	  char     tip_version[12];/*tip_version must not exceed 12 chars*/
	  char     tip_date[12]; /*tip_date must not exceed 12 chars*/
          INT32    max_msg_size;
          INT32    max_bkpts;
};
typedef	struct	versions_etc_t	VERSIONS_ETC;
extern	VERSIONS_ETC	versions_etc;

struct	target_config_t {
          ADDR32   I_mem_start;
          INT32    I_mem_size;
          ADDR32   D_mem_start;
          INT32    D_mem_size;
          ADDR32   ROM_start;
          INT32    ROM_size;
          UINT32    processor_id;
          UINT32    coprocessor;
          INT32    reserved;
};
typedef	struct	target_config_t	TARGET_CONFIG;
extern	TARGET_CONFIG	target_config;

struct	target_status_t {
	  INT32	   status;
          INT32    msgs_sent;
          INT32    msgs_received;
          INT32    errors;
          INT32    bkpts_hit;
          INT32    bkpts_free;
          INT32    traps;
          INT32    fills;
          INT32    spills;
          INT32    cycles;
          INT32    reserved;
};
typedef	struct	target_status_t	TARGET_STATUS;
extern	TARGET_STATUS	target_status;

/*
** Structure a 29K instruction and memory address
*/

struct instr_t {
          BYTE  op;
          BYTE  c;
          BYTE  a;
          BYTE  b;
          };

struct addr_29k_t {
   INT32  memory_space;
   ADDR32 address;
   };

/* The Monitor's stdin, stdout, stderr, at all times */
extern	int	MON_STDIN;
extern	int	MON_STDOUT;
extern	int	MON_STDERR;

/* Variables declared in main.c */
extern	int		QuietMode;
extern	char		*ProgramName;
extern	int		Session_ids[];
extern	int		NumberOfConnections;

/*
** Who controls the keyboard?
*/

#define TERM_29K            (INT32) 0
#define TERM_USER           (INT32) 1

/*
** Processor PRLs
*/

#define PROC_AM29000         0x00
#define PROC_AM29005         0x10
#define PROC_AM29050         0x20
#define PROC_AM29035         0x30
#define PROC_AM29030         0x40
#define PROC_AM29200         0x50
#define PROC_AM29205         0x58
#define	PROC_AM29240	     0x60

#define MESSAGES_ON        0
#define MESSAGES_OFF       1

/* Extern decalarations for global functions defined in main.c */

GLOBAL	void  fatal_error PARAMS((INT32));
GLOBAL	void  warning PARAMS((INT32));

#endif /* _MAIN_H_INCLUDED _ */
