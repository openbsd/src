/* @(#)tdfunc.h	5.18 93/07/30 16:40:16, Srini, AMD */
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
 * This file contains the declarations of the various target dependent
 * functions used by the Message System of Minimon's TIP to communicate
 * with the target.
 *****************************************************************************
 */
 
#ifndef	_TDFUNC_H_INCLUDED_
#define	_TDFUNC_H_INCLUDED_

#include "messages.h"

INT32   init_comm_pceb PARAMS((INT32, INT32));
INT32   msg_send_pceb PARAMS((union msg_t *, INT32));
INT32   msg_recv_pceb PARAMS((union msg_t *, INT32, INT32));
INT32   reset_comm_pceb PARAMS((INT32, INT32));
INT32   exit_comm_pceb PARAMS((INT32, INT32));
INT32   write_memory_pceb PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32   read_memory_pceb PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32	fill_memory_pceb PARAMS((void));
void  go_pceb PARAMS((INT32, INT32));

INT32   init_comm_serial PARAMS((INT32, INT32));
INT32   msg_send_serial PARAMS((union msg_t *, INT32));
#ifdef	MSDOS
INT32   msg_send_parport PARAMS((union msg_t *, INT32));
#endif
INT32   msg_recv_serial PARAMS((union msg_t *, INT32, INT32));
#ifndef	MSDOS
INT32   reset_comm_pcserver PARAMS((INT32, INT32));
#endif
INT32   reset_comm_serial PARAMS((INT32, INT32));
INT32   exit_comm_serial PARAMS((INT32, INT32));
INT32  write_memory_serial PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32   read_memory_serial PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32	fill_memory_serial PARAMS((void));
void  go_serial PARAMS((INT32, INT32));

INT32   init_comm_eb030 PARAMS((INT32, INT32));
INT32   msg_send_eb030 PARAMS((union msg_t *, INT32));
INT32   msg_recv_eb030 PARAMS((union msg_t *, INT32, INT32));
INT32   reset_comm_eb030 PARAMS((INT32, INT32));
INT32   exit_comm_eb030 PARAMS((INT32, INT32));
INT32  write_memory_eb030 PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32   read_memory_eb030 PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32	fill_memory_eb030 PARAMS((void));
void  go_eb030 PARAMS((INT32, INT32));

INT32   init_comm_eb29k PARAMS((INT32, INT32));
INT32   msg_send_eb29k PARAMS((union msg_t *, INT32));
INT32   msg_recv_eb29k PARAMS((union msg_t *, INT32, INT32));
INT32   reset_comm_eb29k PARAMS((INT32, INT32));
INT32   exit_comm_eb29k PARAMS((INT32, INT32));
INT32  write_memory_eb29k PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32   read_memory_eb29k PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32	fill_memory_eb29k PARAMS((void));
void  go_eb29k PARAMS((INT32, INT32));

INT32   init_comm_lcb29k PARAMS((INT32, INT32));
INT32   msg_send_lcb29k PARAMS((union msg_t *, INT32));
INT32   msg_recv_lcb29k PARAMS((union msg_t *, INT32, INT32));
INT32   reset_comm_lcb29k PARAMS((INT32, INT32));
INT32   exit_comm_lcb29k PARAMS((INT32, INT32));
INT32  write_memory_lcb29k PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32   read_memory_lcb29k PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
INT32	fill_memory_lcb29k PARAMS((void));
void  go_lcb29k PARAMS((INT32, INT32));


INT32 send_bfr_serial PARAMS((BYTE *bfr_ptr, INT32 length, INT32 port_base, INT32 *comm_err));

INT32 recv_bfr_serial PARAMS((BYTE *bfr_ptr, INT32 length, INT32 block, INT32 port_base, INT32 *comm_err));
   
#endif /* TDFUNC_H_INCLUDED_ */
