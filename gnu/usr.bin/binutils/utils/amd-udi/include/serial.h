/* @(#)serial.h	5.18 93/07/30 16:40:14, Srini, AMD */
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

#include "messages.h"

int   init_comm_serial PARAMS(());
int   msg_send_serial PARAMS((union msg_t *));
int   msg_recv_serial PARAMS((union msg_t *));
int   reset_comm_serial PARAMS(());
int	read_memory_serial PARAMS(());
int	write_memory_serial PARAMS(());
int	fill_memory_serial PARAMS(());
void  go_serial PARAMS(());

int   init_comm_pceb PARAMS(());
int   msg_send_pceb PARAMS((union msg_t *));
int   msg_recv_pceb PARAMS((union msg_t *));
int   reset_comm_pceb PARAMS(());
int	read_memory_pceb PARAMS(());
int	write_memory_pceb PARAMS(());
int	fill_memory_pceb PARAMS(());
void  go_pceb PARAMS(());

int   init_comm_eb29k PARAMS(());
int   msg_send_eb29k PARAMS((union msg_t *));
int   msg_recv_eb29k PARAMS((union msg_t *));
int   reset_comm_eb29k PARAMS(());
int	read_memory_eb29k PARAMS(());
int	write_memory_eb29k PARAMS(());
int	fill_memory_eb29k PARAMS(());
void  go_eb29k PARAMS(());

int   init_comm_lcb29k PARAMS(());
int   msg_send_lcb29k PARAMS((union msg_t *));
int   msg_recv_lcb29k PARAMS((union msg_t *));
int   reset_comm_lcb29k PARAMS(());
int	read_memory_lcb29k PARAMS(());
int	write_memory_lcb29k PARAMS(());
int	fill_memory_lcb29k PARAMS(());
void  go_lcb29k PARAMS(());

int	init_comm_iss PARAMS(());
int	reset_comm_iss PARAMS(());
int	msg_send_iss PARAMS((union msg_t *));
int	msg_recv_iss PARAMS((union msg_t *));
int	read_memory_iss PARAMS(());
int	write_memory_iss PARAMS(());
int	fill_memory_iss PARAMS(());
void	go_iss PARAMS((int));

int   init_comm_custom PARAMS(());
int   msg_send_custom PARAMS((union msg_t *));
int   msg_recv_custom PARAMS((union msg_t *));
int   reset_comm_custom PARAMS(());
int	read_memory_custom PARAMS(());
int	write_memory_custom PARAMS(());
int	fill_memory_custom PARAMS(());
void  go_custom PARAMS(());

int   init_comm_eb030 PARAMS(());
int   msg_send_eb030 PARAMS((union msg_t *));
int   msg_recv_eb030 PARAMS((union msg_t *));
int   reset_comm_eb030 PARAMS(());
int	read_memory_eb030 PARAMS(());
int	write_memory_eb030 PARAMS(());
int	fill_memory_eb030 PARAMS(());
void  go_eb030 PARAMS(());

