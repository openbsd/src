/* @(#)mtip.h	5.19 93/09/08 14:15:22, Srini, AMD */
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
 * This is the header file of mtip.c module.
 *****************************************************************************
 */
#ifndef	_MTIP_H_INCLUDED_
#define	_MTIP_H_INCLUDED_

#include  "types.h"

#define	ILLOP29K	"00000000"

#define	DEFAULT_BAUD_RATE	"9600"

#define	LOAD_BUFFER_SIZE	1024
#define	FROM_BEGINNING		0

#ifdef	MSDOS
#define	DEFAULT_COMM_PORT	"com1:"
#define	DEFAULT_PAR_PORT	"lpt1:"
#else
#define	DEFAULT_COMM_PORT	"/dev/ttya"
#define	DEFAULT_PAR_PORT	""
#endif


#define TRUE                 1
#define FALSE                0

#define	MAXFILENAMELEN	   256

/* Define BIG and LITTLE endian */
#define BIG                  0
#define LITTLE               1

#ifdef MSDOS
#define FILE_OPEN_FLAG   "rb"
#else
#define FILE_OPEN_FLAG   "r"
#endif

#define BKPT_29050       0
#define	BKPT_29050_BTE_0	0
#define	BKPT_29050_BTE_1	1
#define BKPT_29000      -1

#define	MONMaxMemRanges	3    /* Inst, data, Rom */
#define	MONMaxChips	2   /* main cpu & coprocessor */
#define	MONMaxProcessMemRanges	2
#define	MONMaxStacks	2

#define	MONDefaultMemStackSize	0x6000
#define	MONDefaultRegStackSize	0x2000

struct	tip_target_config_t {
          INT32    processor_id;
          INT32    version;
          ADDR32   I_mem_start;
          INT32    I_mem_size;
          ADDR32   D_mem_start;
          INT32    D_mem_size;
          ADDR32   ROM_start;
          INT32    ROM_size;
          INT32    max_msg_size;
          INT32    max_bkpts;
          INT32    coprocessor;
	  int	   P29KEndian;
	  int	   TipEndian;
          INT32    os_version;
};
typedef	struct	tip_target_config_t	TIP_TARGET_CONFIG;
extern	TIP_TARGET_CONFIG	tip_target_config;

struct	tip_target_status_t {
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
typedef	struct	tip_target_status_t	TIP_TARGET_STATUS;
extern	TIP_TARGET_STATUS	tip_target_status;

struct  tip_config_t {
	INT32	PC_port_base;
	INT32	PC_mem_seg;
	char 	baud_rate[10];
	char 	comm_port[15];
	char 	par_port[15];
};
typedef	struct	tip_config_t	TIP_CONFIG;
extern	TIP_CONFIG		tip_config;

typedef unsigned int BreakIdType;
struct tip_break_table {
  BreakIdType		id;
  INT32		space;
  ADDR32	offset;
  INT32		count;
  INT32		type;
  ADDR32	BreakInst;	/* actual instruction */
  struct tip_break_table *next;
};

extern	char	*Msg_Logfile;

void  tip_convert32 PARAMS((BYTE *));
void  tip_convert16 PARAMS((BYTE *));

#ifdef MSDOS
#define	SIGINT_POLL	kbhit();
#else
#define	SIGINT_POLL
#endif
#endif /* _MTIP_H_INCLUDED_ */
