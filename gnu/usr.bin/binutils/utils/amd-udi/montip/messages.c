static char _[] = "@(#)messages.c	5.20 93/08/02 13:23:58, Srini, AMD.";
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
 * This module contains the functions to build and unpack MiniMON29K messages.
 * It also defines the functions to send and receive messages from the
 * 29K target. An array defining the appropriate functions to use for
 * different targets is initialized.
 *****************************************************************************
 */

/* 
 * Definitions of functions that 
 * -initialize the Message System 
 * -send messages to the target
 * -receive messages from the target
 */

#include <stdio.h>
#include <ctype.h>
#ifdef	MSDOS
#include <stdlib.h>
#endif
#include <string.h>
#include "messages.h"
#include "memspcs.h"
#include "tdfunc.h"
#include "mtip.h"

extern	FILE	*MsgFile;
static	int	DebugCoreVersion;

static 	INT32	target_index = 0;	/* Default EB29K */

int	lpt_initialize=0;	/* global */
int	use_parport=0;	/* global */

static	union msg_t	*send_msg_buffer;
static	union msg_t	*recv_msg_buffer;

struct	target_dep_funcs {
 char	target_name[15];
 INT32	(*msg_send)PARAMS((union msg_t *, INT32));
 INT32	(*msg_recv)PARAMS((union msg_t *, INT32, INT32));
 INT32	(*init_comm)PARAMS((INT32, INT32));
 INT32	(*reset_comm)PARAMS((INT32, INT32));
 INT32	(*exit_comm)PARAMS((INT32, INT32));
 INT32	(*read_memory)PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
 INT32	(*write_memory)PARAMS((INT32, ADDR32, BYTE *, INT32, INT32, INT32));
 INT32	(*fill_memory)PARAMS((void));
 INT32	PC_port_base;
 INT32	PC_mem_seg;
 void	(*go)PARAMS((INT32, INT32));
} TDF[] = {
"pceb", msg_send_pceb, msg_recv_pceb, init_comm_pceb,
reset_comm_pceb, exit_comm_pceb, read_memory_pceb, write_memory_pceb, 
fill_memory_pceb, (INT32) 0x240, (INT32) 0xd000, go_pceb,

#ifndef	MSDOS
"pcserver", msg_send_serial, msg_recv_serial, init_comm_serial,
reset_comm_pcserver, exit_comm_serial, read_memory_serial, write_memory_serial,
fill_memory_serial, (INT32) -1 , (INT32) -1, go_serial,
#endif

#ifdef	MSDOS
"paral_1", msg_send_parport, msg_recv_serial, init_comm_serial,
reset_comm_serial, exit_comm_serial, read_memory_serial, write_memory_serial,
fill_memory_serial, (INT32) -1 , (INT32) -1, go_serial,
#endif

"serial", msg_send_serial, msg_recv_serial, init_comm_serial,
reset_comm_serial, exit_comm_serial, read_memory_serial, write_memory_serial,
fill_memory_serial, (INT32) -1 , (INT32) -1, go_serial,

"eb29030", msg_send_eb030, msg_recv_eb030, init_comm_eb030,
reset_comm_eb030, exit_comm_eb030, read_memory_eb030, write_memory_eb030,
fill_memory_eb030, (INT32) 0x208, (INT32) 0xd000, go_eb030,

"eb030", msg_send_eb030, msg_recv_eb030, init_comm_eb030,
reset_comm_eb030, exit_comm_eb030, read_memory_eb030, write_memory_eb030,
fill_memory_eb030, (INT32) 0x208, (INT32) 0xd000, go_eb030,

"eb29k", msg_send_eb29k, msg_recv_eb29k, init_comm_eb29k,
reset_comm_eb29k, exit_comm_eb29k, read_memory_eb29k, write_memory_eb29k, 
fill_memory_eb29k, (INT32) 0x208, (INT32) 0xd000, go_eb29k,

"yarcrev8", msg_send_eb29k, msg_recv_eb29k, init_comm_eb29k,
reset_comm_eb29k, exit_comm_eb29k, read_memory_eb29k, write_memory_eb29k, 
fill_memory_eb29k, (INT32) 0x208, (INT32) 0xd000, go_eb29k,

"lcb29k", msg_send_lcb29k, msg_recv_lcb29k, init_comm_lcb29k, 
reset_comm_lcb29k, exit_comm_lcb29k, read_memory_lcb29k, write_memory_lcb29k,
fill_memory_lcb29k, (INT32) 0x208, (INT32) 0xd000, go_lcb29k,

"\0"
};

void	print_msg PARAMS((union msg_t *msgptr, FILE *file));
static	INT32	match_name PARAMS((char *targ_name));


#ifdef MSDOS
void	set_lpt PARAMS((void));
void	unset_lpt PARAMS((void));

void	set_lpt ()
{
  TDF[target_index].msg_send = msg_send_parport;
  use_parport = 1;
}

void	unset_lpt()
{
  TDF[target_index].msg_send = msg_send_serial;
  use_parport = 0;
}

#endif
/*
** Miscellaneous 
*/

INT32 msg_length(code)
INT32 code;
{  /* for temporary compatibility between new and old r/w/copy msgs */
INT32 rv;
  if (code == WRITE_REQ) 
      rv = MSG_LENGTH(struct write_req_msg_t);
  else
  if (code == READ_REQ) 
      rv = MSG_LENGTH(struct read_req_msg_t);
  else
  if (code == COPY) 
      rv = MSG_LENGTH(struct copy_msg_t);
  else return(-1);

  /* if msg version < 0x10 use old format */
  /* assumes config info this has been set up */
  if (((tip_target_config.version >> 16) & 0xff) < 0x10) 
	rv = rv - 4;		 
  return(rv);
}

/*
** Target Dependent functions
*/

INT32
Mini_msg_init(target_name)
char	*target_name;
{
  INT32		temp;

  /* Allocate buffers */
  if ((send_msg_buffer = (union msg_t *) malloc (BUFFER_SIZE)) == NULL)
    return(FAILURE);
  if ((recv_msg_buffer = (union msg_t *) malloc (BUFFER_SIZE)) == NULL)
    return(FAILURE);

  /* Identify target */
  if (strcmp (target_name, "paral_1") == 0) {
    lpt_initialize = 1;
    use_parport = 1;
  }

  if ((temp = match_name(target_name)) == FAILURE)
    return(FAILURE);  /* Unrecognized target */
  else
    target_index = temp;

  if (tip_config.PC_port_base == (INT32) -1) /* no -port opt given */
     tip_config.PC_port_base = TDF[target_index].PC_port_base;

  if (tip_config.PC_mem_seg == (INT32) -1) /* no -seg opt given */
     tip_config.PC_mem_seg = TDF[target_index].PC_mem_seg;

  /* Initialize communication with target */
  return(Mini_init_comm());

}

int
Mini_alloc_msgbuf(size)
int	size;
{
  if (size > (int) BUFFER_SIZE) {
     (void) free(send_msg_buffer);
     (void) free(recv_msg_buffer);

     /* Re-Allocate buffers */
     if ((send_msg_buffer = (union msg_t *) malloc (size)) == NULL)
       return(FAILURE);
     if ((recv_msg_buffer = (union msg_t *) malloc (size)) == NULL)
       return(FAILURE);
  }
  return (SUCCESS);
}

void
Mini_msg_exit()
{
  if (send_msg_buffer)
    (void) free ((char *) send_msg_buffer);
  if (recv_msg_buffer)
    (void) free ((char *) recv_msg_buffer);

  (void) Mini_reset_comm();
  (void) Mini_exit_comm();
}

INT32
Mini_msg_send()
{
  INT32	retval;

  if (Msg_Logfile) {/* log the message */
     fprintf(MsgFile, "\nSending:");
     print_msg(send_msg_buffer, MsgFile);
     fflush(MsgFile);
  };
  retval = (*TDF[target_index].msg_send)(send_msg_buffer,
				       tip_config.PC_port_base);
  /* retry once more */
  if (retval == MSGRETRY)
     retval = (*TDF[target_index].msg_send)(send_msg_buffer,
				       tip_config.PC_port_base);

  return (retval);
}

INT32
Mini_msg_recv(RecvMode)
INT32	RecvMode;	/* BLOCK or NONBLOCK */
{
  INT32	retval;

  retval = (INT32) (*TDF[target_index].msg_recv)(recv_msg_buffer,
				       tip_config.PC_port_base, RecvMode);
  if (RecvMode == BLOCK)  /* we are expecting a response */
  {
     if (retval == MSGRETRY) {
	Mini_msg_send();
        retval = (INT32) (*TDF[target_index].msg_recv)(recv_msg_buffer,
				       tip_config.PC_port_base, RecvMode);
     }
     if (Msg_Logfile && (retval != (INT32) -1)) { /* log the message */
       fprintf(MsgFile, "\nReceived:");
       print_msg(recv_msg_buffer, MsgFile);
       fflush (MsgFile);
     };
     if (retval == MSGRETRY)
	return (FAILURE);
     else 
	return (retval);
  }
  else 	/* non-block mode */
  {
     if (retval == MSGRETRY) {
        retval = (INT32) (*TDF[target_index].msg_recv)(recv_msg_buffer,
				       tip_config.PC_port_base, RecvMode);
	if (retval == MSGRETRY)
	  return (FAILURE);
	else
	  return (retval);
     } else  {
        if (Msg_Logfile && (retval != (INT32) -1)) { /* log the message */
          fprintf(MsgFile, "\nReceived:");
          print_msg(recv_msg_buffer, MsgFile);
          fflush (MsgFile);
        };
        return (retval);
     }
  }
}

INT32
Mini_init_comm()
{
 return((*TDF[target_index].init_comm)(tip_config.PC_port_base,
				       tip_config.PC_mem_seg));
}

INT32
Mini_reset_comm()
{
 return((*TDF[target_index].reset_comm)(tip_config.PC_port_base,
					tip_config.PC_mem_seg));
}

INT32
Mini_exit_comm()
{
 return((*TDF[target_index].exit_comm)(tip_config.PC_port_base,
					tip_config.PC_mem_seg));
}


void
Mini_go_target()
{
 (*TDF[target_index].go)(tip_config.PC_port_base,
				 tip_config.PC_mem_seg);
}

INT32
Mini_write_memory(m_space, address, byte_count, buffer)
INT32	m_space;
ADDR32	address;
INT32	byte_count;
BYTE	*buffer;
{
 return((*TDF[target_index].write_memory)(m_space, 
					  address,
					  buffer,
					  byte_count,
					  tip_config.PC_port_base,
					  tip_config.PC_mem_seg));
}

INT32
Mini_read_memory(m_space, address, byte_count, buffer)
INT32	m_space;
ADDR32	address;
INT32	byte_count;
BYTE	*buffer;
{
 return((*TDF[target_index].read_memory)(m_space,
					  address,
					  buffer,
					  byte_count,
					  tip_config.PC_port_base,
					  tip_config.PC_mem_seg));
}

INT32
Mini_fill_memory()
{
 return((*TDF[target_index].fill_memory)());
}

/* 
** Functions to build msgs
*/

void
Mini_build_reset_msg()
{
 send_msg_buffer->reset_msg.code = RESET;
 send_msg_buffer->reset_msg.length = (INT32) 0;/* Length always is zero */
}

void
Mini_build_config_req_msg()
{
 send_msg_buffer->config_req_msg.code = CONFIG_REQ;
 send_msg_buffer->config_req_msg.length = (INT32) 0; /* Always zero */
}

void
Mini_build_status_req_msg()
{
send_msg_buffer->status_req_msg.code = STATUS_REQ;
send_msg_buffer->status_req_msg.length = (INT32) 0; /* Always zero */
}

void
Mini_build_read_req_msg(m_space, address, count, size)
INT32	m_space;
ADDR32	address;
INT32	count;
INT32	size;
{
send_msg_buffer->read_req_msg.code = READ_REQ;
send_msg_buffer->read_req_msg.length = msg_length(READ_REQ);
send_msg_buffer->read_req_msg.memory_space = m_space;
if ((DebugCoreVersion >= (int) 0x13) && (m_space == (INT32) SPECIAL_REG))
   send_msg_buffer->read_req_msg.memory_space = (INT32) A_SPCL_REG;
send_msg_buffer->read_req_msg.address = address;
/* if msg version >= 0x10 use new format, else old */
if (((tip_target_config.version >> 16) & 0xff) >= 0x10) { /* new version */
	send_msg_buffer->read_req_msg.count = count;
	send_msg_buffer->read_req_msg.size = size;
	} else {					/* old version */
	send_msg_buffer->read_req_msg.count = count * size;
	}
}

void
Mini_build_write_req_msg(m_space, address, count, size, data)
INT32	m_space;
ADDR32	address;
INT32	count;
INT32	size;
BYTE	*data;
{
  BYTE		*s;
  INT32		i;
  INT32		bcnt = count * size;

send_msg_buffer->write_req_msg.code = WRITE_REQ;
send_msg_buffer->write_req_msg.length = msg_length(WRITE_REQ) + (count * size);
send_msg_buffer->write_req_msg.memory_space = m_space;
if ((DebugCoreVersion >= (int) 0x13) && (m_space == (INT32) SPECIAL_REG))
   send_msg_buffer->write_req_msg.memory_space = (INT32) A_SPCL_REG;
send_msg_buffer->write_req_msg.address = address;

/* if msg version >= 0x10 use new format, else old */
if (((tip_target_config.version >> 16) & 0xff) >= 0x10) { /* new version */
	send_msg_buffer->write_req_msg.count = count;
	send_msg_buffer->write_req_msg.size = size;
	s = &(send_msg_buffer->write_req_msg.data);
	for (i = 0; i < bcnt; i++)
	  *s++ = *data++;
	} else { 					/* old version */
	send_msg_buffer->write_req_msg.count = bcnt;
	s = (BYTE *) &(send_msg_buffer->write_req_msg.size);
	for (i = 0; i < bcnt; i++)
	  *s++ = *data++;
	}
}

void
Mini_build_bkpt_set_msg(m_space, address, pass_count, type)
INT32	m_space, pass_count, type;
ADDR32	address;
{
send_msg_buffer->bkpt_set_msg.code = BKPT_SET;
send_msg_buffer->bkpt_set_msg.length = MSG_LENGTH (struct bkpt_set_msg_t);
send_msg_buffer->bkpt_set_msg.memory_space = m_space;
send_msg_buffer->bkpt_set_msg.bkpt_addr = address;
send_msg_buffer->bkpt_set_msg.pass_count = pass_count;
send_msg_buffer->bkpt_set_msg.bkpt_type = type;
}

void
Mini_build_bkpt_rm_msg(m_space, address)
INT32	m_space;
ADDR32	address;
{
send_msg_buffer->bkpt_rm_msg.code = BKPT_RM;
send_msg_buffer->bkpt_rm_msg.length = MSG_LENGTH (struct bkpt_rm_msg_t);
send_msg_buffer->bkpt_rm_msg.memory_space = m_space;
send_msg_buffer->bkpt_rm_msg.bkpt_addr = address;
}

void
Mini_build_bkpt_stat_msg(m_space, address)
INT32	m_space;
ADDR32	address;
{
send_msg_buffer->bkpt_stat_msg.code = BKPT_STAT;
send_msg_buffer->bkpt_stat_msg.length = MSG_LENGTH (struct bkpt_stat_msg_t);
send_msg_buffer->bkpt_stat_msg.memory_space = m_space;
send_msg_buffer->bkpt_stat_msg.bkpt_addr = address;
}

void
Mini_build_copy_msg(src_space, src_addr, dst_space, dst_addr, count, size)
INT32	src_space, dst_space;
ADDR32	src_addr, dst_addr;
INT32	count;
INT32	size;
{
send_msg_buffer->copy_msg.code = COPY;
send_msg_buffer->copy_msg.length = msg_length(COPY);
send_msg_buffer->copy_msg.source_space = src_space;
if ((DebugCoreVersion >= (int) 0x13) && (src_space == (INT32) SPECIAL_REG))
   send_msg_buffer->copy_msg.source_space = (INT32) A_SPCL_REG;
send_msg_buffer->copy_msg.source_addr = src_addr;
send_msg_buffer->copy_msg.dest_space = dst_space;
if ((DebugCoreVersion >= (int) 0x13) && (dst_space == (INT32) SPECIAL_REG))
   send_msg_buffer->copy_msg.dest_space = (INT32) A_SPCL_REG;
send_msg_buffer->copy_msg.dest_addr = dst_addr;

/* if msg version >= 0x10 use new format, else old */
if (((tip_target_config.version >> 16) & 0xff) >= 0x10) { /* new version */
	send_msg_buffer->copy_msg.count = count;
	send_msg_buffer->copy_msg.size = size;
	} else {					/* old version */
	send_msg_buffer->copy_msg.count = count * size;
	}
}

void
Mini_build_fill_msg(m_space, start, fill_count, byte_count, pattern)
INT32	m_space;
ADDR32	start;
INT32	fill_count, byte_count;
BYTE	*pattern;
{
send_msg_buffer->fill_msg.code = FILL;
send_msg_buffer->fill_msg.length = MSG_LENGTH (struct fill_msg_t) +
					byte_count; 
send_msg_buffer->fill_msg.memory_space = m_space;
if ((DebugCoreVersion >= (int) 0x13) && (m_space == (INT32) SPECIAL_REG))
   send_msg_buffer->fill_msg.memory_space = (INT32) A_SPCL_REG;
send_msg_buffer->fill_msg.start_addr = start;
send_msg_buffer->fill_msg.fill_count = fill_count;
send_msg_buffer->fill_msg.byte_count = byte_count;
(void) strcpy ( &(send_msg_buffer->fill_msg.fill_data),pattern);
}

void
Mini_build_init_msg(t_start, t_end, d_start, d_end, 
		    entry, m_stack, r_stack, 
		    highmem, arg_start, os_ctrl)
ADDR32	t_start, t_end, d_start, d_end;
ADDR32	entry, highmem, arg_start;
INT32	m_stack, r_stack;
INT32	os_ctrl;
{
send_msg_buffer->init_msg.code = INIT;
/* subtract 4 to hide highmem value */
send_msg_buffer->init_msg.length = MSG_LENGTH (struct init_msg_t) - 4;
send_msg_buffer->init_msg.text_start = t_start;
send_msg_buffer->init_msg.text_end = t_end;
send_msg_buffer->init_msg.data_start = d_start;
send_msg_buffer->init_msg.data_end = d_end;
send_msg_buffer->init_msg.entry_point = entry;
send_msg_buffer->init_msg.mem_stack_size = m_stack;
send_msg_buffer->init_msg.reg_stack_size = r_stack;
send_msg_buffer->init_msg.arg_start = arg_start;
send_msg_buffer->init_msg.os_control = os_ctrl;
send_msg_buffer->init_msg.highmem = highmem;
}

void
Mini_build_go_msg()
{
send_msg_buffer->go_msg.code = GO;
send_msg_buffer->go_msg.length = (INT32) 0; /* Always zero */
}

void
Mini_build_step_msg(count)
INT32	count;
{
send_msg_buffer->step_msg.code = STEP;
send_msg_buffer->step_msg.length = sizeof(INT32);
send_msg_buffer->step_msg.count = count;
}

void
Mini_build_break_msg()
{
send_msg_buffer->break_msg.code = BREAK;
send_msg_buffer->break_msg.length = (INT32) 0; /* Always zero */
}

void
Mini_build_hif_rtn_msg(snum, gr121, gr96, gr97)
INT32	snum, gr121, gr96, gr97;
{
send_msg_buffer->hif_call_rtn_msg.code = HIF_CALL_RTN;
send_msg_buffer->hif_call_rtn_msg.length = MSG_LENGTH (struct hif_call_rtn_msg_t);
send_msg_buffer->hif_call_rtn_msg.service_number = snum;
send_msg_buffer->hif_call_rtn_msg.gr121 = gr121;
send_msg_buffer->hif_call_rtn_msg.gr96 = gr96;
send_msg_buffer->hif_call_rtn_msg.gr97 = gr97;
}

void
Mini_build_channel0_msg(input, count)
INT32	count;
BYTE	*input;
{
send_msg_buffer->channel0_msg.code = CHANNEL0;
send_msg_buffer->channel0_msg.length = count;	/* bytes to follow */
(void ) memcpy (&(send_msg_buffer->channel0_msg.data), input, (int) count);
}

void
Mini_build_channel1_ack_msg(gr96)
INT32	gr96;
{
send_msg_buffer->channel1_ack_msg.code = CHANNEL1_ACK;
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
     if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
	/*
	 * The CHANNEL1_ACK for new HIF kernel includes the gr96 value
	 * which is the number of characters succesfully printed out.
	 */
       send_msg_buffer->channel1_ack_msg.length = (INT32) 4; /* return gr96 */
       send_msg_buffer->channel1_ack_msg.gr96 = gr96;
     } else { /* old HIF kernel */
       send_msg_buffer->channel1_ack_msg.length = (INT32) 0; 
     }
}

void
Mini_build_channel2_ack_msg(gr96)
INT32	gr96;
{
send_msg_buffer->channel2_ack_msg.code = CHANNEL2_ACK;
	/*
	 * The HIF kernel from MiniMON29K release 2.1 expects MONTIP
	 * to send a HIF_CALL_RTN response for a HIF_CALL message, and
	 * a CHANNEL1_ACK response for a CHANNEL1 message, and 
	 * a CHANNEL2_ACK response for a CHANNEL2 message, and
	 * a CHANNEL0 message for a asynchronous input.
	 * The HIF kernel version numbers 0x05 and above support these
	 * features.
	 */
     if ((tip_target_config.os_version & 0xf) > 4) { /* new HIF kernel */
	/*
	 * The CHANNEL1_ACK for new HIF kernel includes the gr96 value
	 * which is the number of characters succesfully printed out.
	 */
       send_msg_buffer->channel2_ack_msg.length = (INT32) 4; /* return gr96 */
       send_msg_buffer->channel2_ack_msg.gr96 = gr96;
     } else { /* old HIF kernel */
       /* 
	* The old kernels did not support this feature. They invoked the
	* debugger on target to get the information.
	*/
     }
}

void	Mini_build_stdin_needed_ack_msg (count, data)
UINT32	count;
BYTE	*data;
{
  BYTE	*s;

send_msg_buffer->stdin_needed_ack_msg.code = STDIN_NEEDED_ACK;
send_msg_buffer->stdin_needed_ack_msg.length = (INT32) count;
s = &(send_msg_buffer->stdin_needed_ack_msg.data);
for (; count > 0; count--)
  *s++ = *data++;
}

void	Mini_build_stdin_mode_ack_msg (mode)
INT32	mode;
{
send_msg_buffer->stdin_mode_ack_msg.code = STDIN_MODE_ACK;
send_msg_buffer->stdin_mode_ack_msg.length = MSG_LENGTH(struct stdin_mode_ack_msg_t);
send_msg_buffer->stdin_mode_ack_msg.mode = mode;
}

/*
** Functions to unpack messages.
*/

void
Mini_unpack_reset_ack_msg()
{
 /* No data in this message */
}

void
Mini_unpack_config_msg(target_config)
TIP_TARGET_CONFIG	*target_config;
{
  /* received a CONFIG message */
  target_config->processor_id = recv_msg_buffer->config_msg.processor_id;
  target_config->version =  recv_msg_buffer->config_msg.version;
  DebugCoreVersion = (int) (target_config->version & 0xFF);
  target_config->I_mem_start =  recv_msg_buffer->config_msg.I_mem_start;
  target_config->I_mem_size =  recv_msg_buffer->config_msg.I_mem_size;
  target_config->D_mem_start =  recv_msg_buffer->config_msg.D_mem_start;
  target_config->D_mem_size =  recv_msg_buffer->config_msg.D_mem_size;
  target_config->ROM_start =  recv_msg_buffer->config_msg.ROM_start;
  target_config->ROM_size =  recv_msg_buffer->config_msg.ROM_size;
  target_config->max_msg_size =  recv_msg_buffer->config_msg.max_msg_size;
  target_config->max_bkpts =  recv_msg_buffer->config_msg.max_bkpts;
  target_config->coprocessor =  recv_msg_buffer->config_msg.coprocessor;
  target_config->os_version =  recv_msg_buffer->config_msg.os_version;
}

void
Mini_unpack_status_msg(target_status)
TIP_TARGET_STATUS	*target_status;
{
  /* received a STATUS mesages */
  target_status->msgs_sent = recv_msg_buffer->status_msg.msgs_sent;
  target_status->msgs_received = recv_msg_buffer->status_msg.msgs_received;
  target_status->errors = recv_msg_buffer->status_msg.errors;
  target_status->bkpts_hit = recv_msg_buffer->status_msg.bkpts_hit;
  target_status->bkpts_free = recv_msg_buffer->status_msg.bkpts_free;
  target_status->traps = recv_msg_buffer->status_msg.traps;
  target_status->fills = recv_msg_buffer->status_msg.fills;
  target_status->spills = recv_msg_buffer->status_msg.spills;
  target_status->cycles = recv_msg_buffer->status_msg.cycles;
}

void
Mini_unpack_read_ack_msg(mspace, address, bytecount, buffer)
INT32	*mspace;
ADDR32	*address;
INT32	*bytecount;
BYTE	*buffer;
{
 INT32		i;
 BYTE		*s;

 /* READ_ACK received */
 *mspace = recv_msg_buffer->read_ack_msg.memory_space;
 if ((DebugCoreVersion >= (int) 0x13) && (*mspace == (INT32) A_SPCL_REG))
   *mspace = (INT32) SPECIAL_REG;
 *address = recv_msg_buffer->read_ack_msg.address;
 *bytecount = recv_msg_buffer->read_ack_msg.byte_count;
  s = &(recv_msg_buffer->read_ack_msg.data);
  for (i = 0; i < *bytecount; i++)
     *buffer++ = *s++;
}

void
Mini_unpack_write_ack_msg(mspace, address, bytecount)
INT32	*mspace;
ADDR32	*address;
INT32	*bytecount;
{
  *mspace =recv_msg_buffer->write_ack_msg.memory_space;
  if ((DebugCoreVersion >= (int) 0x13) && (*mspace == (INT32) A_SPCL_REG))
     *mspace = (INT32) SPECIAL_REG;
  *address =recv_msg_buffer->write_ack_msg.address;
  *bytecount =recv_msg_buffer->write_ack_msg.byte_count;
}

void
Mini_unpack_bkpt_set_ack_msg(mspace, address, passcount, bkpt_type)
INT32	*mspace;
ADDR32	*address;
INT32	*passcount;
INT32	*bkpt_type;
{
  *mspace =recv_msg_buffer->bkpt_set_ack_msg.memory_space;
  *address =recv_msg_buffer->bkpt_set_ack_msg.address;
  *passcount =recv_msg_buffer->bkpt_set_ack_msg.pass_count;
  *bkpt_type =recv_msg_buffer->bkpt_set_ack_msg.bkpt_type;
}

void
Mini_unpack_bkpt_rm_ack_msg(mspace, address)
INT32	*mspace;
ADDR32	*address;
{
  *mspace = recv_msg_buffer->bkpt_rm_ack_msg.memory_space;
  *address = recv_msg_buffer->bkpt_rm_ack_msg.address;
}

void
Mini_unpack_bkpt_stat_ack_msg(mspace, address, pass_count, bkpt_type)
INT32	*mspace;
ADDR32	*address;
INT32	*pass_count;
INT32	*bkpt_type;
{
  *mspace = recv_msg_buffer->bkpt_stat_ack_msg.memory_space;
  *address = recv_msg_buffer->bkpt_stat_ack_msg.address;
  *pass_count = recv_msg_buffer->bkpt_stat_ack_msg.pass_count;
  *bkpt_type = recv_msg_buffer->bkpt_stat_ack_msg.bkpt_type;
}

void
Mini_unpack_copy_ack_msg(srcspace, srcaddr, dstspace, dstaddr, count)
INT32	*srcspace, *dstspace;
ADDR32	*srcaddr, *dstaddr;
INT32	*count;
{
  *srcspace = recv_msg_buffer->copy_ack_msg.source_space;
  if ((DebugCoreVersion >= (int) 0x13) && (*srcspace == (INT32) A_SPCL_REG))
   *srcspace = (INT32) SPECIAL_REG;
  *srcaddr = recv_msg_buffer->copy_ack_msg.source_addr;
  *dstspace = recv_msg_buffer->copy_ack_msg.dest_space;
  if ((DebugCoreVersion >= (int) 0x13) && (*dstspace == (INT32) A_SPCL_REG))
   *dstspace = (INT32) SPECIAL_REG;
  *dstaddr = recv_msg_buffer->copy_ack_msg.dest_addr;
  *count = recv_msg_buffer->copy_ack_msg.byte_count;
}

void
Mini_unpack_fill_ack_msg(mspace, startaddr, fillcount, pattern_cnt)
INT32	*mspace;
ADDR32	*startaddr;
INT32	*fillcount;
INT32	*pattern_cnt;
{
  *mspace = recv_msg_buffer->fill_ack_msg.memory_space;
  if ((DebugCoreVersion >= (int) 0x13) && (*mspace == (INT32) A_SPCL_REG))
    *mspace = (INT32) SPECIAL_REG;
  *startaddr = recv_msg_buffer->fill_ack_msg.start_addr;
  *fillcount = recv_msg_buffer->fill_ack_msg.fill_count;
  *pattern_cnt = recv_msg_buffer->fill_ack_msg.byte_count;
}

void
Mini_unpack_init_ack_msg()
{
 /* No data in this message */

}

void
Mini_unpack_halt_msg(mspace, pc0, pc1, trap_number)
INT32	*mspace;
ADDR32	*pc0;
ADDR32	*pc1;
INT32	*trap_number;
{
  *mspace = recv_msg_buffer->halt_msg.memory_space;
  *pc0 = recv_msg_buffer->halt_msg.pc0;
  *pc1 = recv_msg_buffer->halt_msg.pc1;
  *trap_number = recv_msg_buffer->halt_msg.trap_number;
}

void
Mini_unpack_error_msg(errcode, mspace, address)
INT32	*errcode;
INT32	*mspace;
ADDR32	*address;
{
  *errcode = recv_msg_buffer->error_msg.error_code;
  *mspace = recv_msg_buffer->error_msg.memory_space;
  *address = recv_msg_buffer->error_msg.address;
}

void
Mini_unpack_channel0_ack_msg()
{
 /* No data in this message */
}

void
Mini_unpack_channel2_msg(data, len)
BYTE	*data;
INT32	*len;
{
  INT32	i;
  BYTE	*s;

  *len = recv_msg_buffer->channel2_msg.length;
  s = &(recv_msg_buffer->channel2_msg.data);
  for (i = 0; i < *len; i++)
     *data++ = *s++;
}

void
Mini_unpack_channel1_msg(data, len)
BYTE	*data;
INT32	*len;
{
  INT32	i;
  BYTE	*s;

  *len = recv_msg_buffer->channel1_msg.length;
  s = &(recv_msg_buffer->channel1_msg.data);
  for (i = 0; i < *len; i++)
     *data++ = *s++;
}

void	
Mini_unpack_hif_msg (gr121, lr2, lr3, lr4)
INT32 *gr121;
INT32 *lr2;
INT32 *lr3;
INT32 *lr4;
{  
  *gr121 = recv_msg_buffer->hif_call_msg.service_number;
  *lr2 = recv_msg_buffer->hif_call_msg.lr2;
  *lr3 = recv_msg_buffer->hif_call_msg.lr3;
  *lr4 = recv_msg_buffer->hif_call_msg.lr4;
}

void	Mini_unpack_stdin_needed_msg (nbytes)
INT32	*nbytes;
{
  *nbytes = recv_msg_buffer->stdin_needed_msg.nbytes;
}

void	Mini_unpack_stdin_mode_msg (mode)
INT32	*mode;
{
  *mode = recv_msg_buffer->stdin_mode_msg.mode;
}


/* miscellaneous */

static
INT32 match_name(name)
char	*name;
{
 int	i;

 i = 0;
 while (TDF[i].target_name) {
   if (strcmp(TDF[i].target_name, name))
     i++;
   else
     return((INT32) i);
 }
 return(FAILURE);
}

/*
** This function is used to print out a message which has
** been received from the target.
*/

void
print_msg(msg, MsgFile)
   union msg_t *msg;
   FILE		*MsgFile;
   {
   INT32  i, j;
   INT32  length;
   BYTE		*s;
   INT32	*hex;
   INT32	code;

   hex = &(msg->generic_int32_msg.int32);
   s = &(msg->generic_msg.byte);
   length = msg->generic_msg.length;

   fprintf(MsgFile, "\n");
   code = msg->generic_msg.code;
   fprintf(MsgFile, "Code:    %ld ", code);
   switch (code) {
     case	RESET:
	fprintf(MsgFile,"(RESET)\t");
	break;
     case	CONFIG_REQ:
	fprintf(MsgFile,"(CONFIG_REQ)\t");
	break;
     case	STATUS_REQ:
	fprintf(MsgFile,"(STATUS_REQ)\t");
	break;
     case	READ_REQ:
	fprintf(MsgFile,"(READ_REQ)\t");
	break;
     case	WRITE_REQ:
	fprintf(MsgFile,"(WRITE_REQ)\t");
	break;
     case	BKPT_SET:
	fprintf(MsgFile,"(BKPT_SET)\t");
	break;
     case	BKPT_RM:
	fprintf(MsgFile,"(BKPT_RM)\t");
	break;
     case	BKPT_STAT:
	fprintf(MsgFile,"(BKPT_STAT)\t");
	break;
     case	COPY:
	fprintf(MsgFile,"(COPY)\t");
	break;
     case	FILL:
	fprintf(MsgFile,"(FILL)\t");
	break;
     case	INIT:
	fprintf(MsgFile,"(INIT)\t");
	break;
     case	GO:
	fprintf(MsgFile,"(GO)\t");
	break;
     case	STEP:
	fprintf(MsgFile,"(STEP)\t");
	break;
     case	BREAK:
	fprintf(MsgFile,"(BREAK)\t");
	break;
     case	HIF_CALL_RTN:
	fprintf(MsgFile,"(HIF_CALL_RTN)\t");
	break;
     case	CHANNEL0:
	fprintf(MsgFile,"(CHANNEL0)\t");
	break;
     case	CHANNEL1_ACK:
	fprintf(MsgFile,"(CHANNEL1_ACK)\t");
	break;
     case	CHANNEL2_ACK:
	fprintf(MsgFile,"(CHANNEL2_ACK)\t");
	break;
     case	STDIN_NEEDED_ACK:
	fprintf(MsgFile,"(STDIN_NEEDED_ACK)\t");
	break;
     case	STDIN_MODE_ACK:
	fprintf(MsgFile,"(STDIN_MODE_ACK)\t");
	break;
     case	RESET_ACK:
	fprintf(MsgFile,"(RESET_ACK)\t");
	break;
     case	CONFIG:
	fprintf(MsgFile,"(CONFIG)\t");
	break;
     case	STATUS:
	fprintf(MsgFile,"(STATUS)\t");
	break;
     case	READ_ACK:
	fprintf(MsgFile,"(READ_ACK)\t");
	break;
     case	WRITE_ACK:
	fprintf(MsgFile,"(WRITE_ACK)\t");
	break;
     case	BKPT_SET_ACK:
	fprintf(MsgFile,"(BKPT_SET_ACK)\t");
	break;
     case	BKPT_RM_ACK:
	fprintf(MsgFile,"(BKPT_RM_ACK)\t");
	break;
     case	BKPT_STAT_ACK:
	fprintf(MsgFile,"(BKPT_STAT_ACK)\t");
	break;
     case	COPY_ACK:
	fprintf(MsgFile,"(COPY_ACK)\t");
	break;
     case	FILL_ACK:
	fprintf(MsgFile,"(FILL_ACK)\t");
	break;
     case	INIT_ACK:
	fprintf(MsgFile,"(INIT_ACK)\t");
	break;
     case	HALT:
	fprintf(MsgFile,"(HALT)\t");
	break;
     case	ERROR:
	fprintf(MsgFile,"(ERROR)\t");
	break;
     case	HIF_CALL:
	fprintf(MsgFile,"(HIF_CALL)\t");
	break;
     case	CHANNEL0_ACK:
	fprintf(MsgFile,"(CHANNEL0_ACK)\t");
	break;
     case	CHANNEL1:
	fprintf(MsgFile,"(CHANNEL1)\t");
	break;
     case	CHANNEL2:
	fprintf(MsgFile,"(CHANNEL2)\t");
	break;
     case	STDIN_NEEDED_REQ:
	fprintf(MsgFile,"(STDIN_NEEDED_REQ)\t");
	break;
     case	STDIN_MODE_REQ:
	fprintf(MsgFile,"(STDIN_MODE_REQ)\t");
	break;
     default:
	fprintf(MsgFile,"(unknown)\t");
	break;
   }
   fprintf(MsgFile, "Length:  %ld\n", msg->generic_msg.length);
   if ((code == CHANNEL1) || (code == CHANNEL2))
       return;
   if ((code == WRITE_REQ) || (code == FILL)) length = 20;
   if (code == READ_ACK) length = 16;
   if (code == STDIN_NEEDED_ACK) length = 16;
   for (i=0; i<((length+sizeof(INT32)-1)/sizeof(INT32)); i=i+1) {
      fprintf(MsgFile, "%08lx  (",  *hex++);
      for (j=0; j<sizeof(INT32); j=j+1)
         if (isprint(*s))
            fprintf(MsgFile, "%d", *s++);
               else
                  s++, fprintf(MsgFile, ".");
      fprintf(MsgFile, ")\n");
      }

   }  /* end print_msg() */




void
CopyMsgToTarg(source)
union msg_t	*source;
{
  INT32		msglen;
  INT32		count;
  char		*to, *from;

  send_msg_buffer->generic_msg.code = source->generic_msg.code;
  send_msg_buffer->generic_msg.length = source->generic_msg.length;
  msglen = source->generic_msg.length;
  to = (char *) &(send_msg_buffer->generic_msg.byte);
  from = (char *) &(source->generic_msg.byte);
  for (count = (INT32) 0; count < msglen; count++)
     *to++ = *from++;

}

void
CopyMsgFromTarg(dest)
union msg_t	*dest;
{
  INT32		msglen;
  INT32		count;
  char		*to, *from;

  dest->generic_msg.code = recv_msg_buffer->generic_msg.code;
  dest->generic_msg.length = recv_msg_buffer->generic_msg.length;
  msglen = recv_msg_buffer->generic_msg.length;
  to = (char *) &(dest->generic_msg.byte);
  from = (char *) &(recv_msg_buffer->generic_msg.byte);
  for (count = (INT32) 0; count < msglen; count++)
     *to++ = *from++;

}

void
print_recv_bytes()
{
  printf("Bytes received: \n");
  printf("0x%lx \n", (long) recv_msg_buffer->generic_msg.code);
  printf("0x%lx \n", (long) recv_msg_buffer->generic_msg.length);
}

