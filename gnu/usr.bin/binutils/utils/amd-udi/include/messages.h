/* @(#)messages.h	5.19 93/08/10 17:49:09, Srini, AMD */
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
 **       This header file describes the messages which are passed
 **       between the target and the host.  This file basically defines
 **       a variant record of type msg_t.
 **
 **       Note that the messages use the types defined in the "types.h"
 **       header file. 
 *****************************************************************************
 */

#ifndef	_MESSAGES_H_INCLUDED_
#define	_MESSAGES_H_INCLUDED_

#include	"types.h"
#include	"mtip.h"

/*
** Host to target definitions
*/

#define RESET          0
#define CONFIG_REQ     1
#define STATUS_REQ     2
#define READ_REQ       3
#define WRITE_REQ      4
#define BKPT_SET       5
#define BKPT_RM        6
#define BKPT_STAT      7
#define COPY           8
#define FILL           9
#define INIT          10
#define GO            11
#define STEP          12
#define BREAK         13

#define HIF_CALL_RTN  64
#define CHANNEL0      65
#define CHANNEL1_ACK  66
#define CHANNEL2_ACK  67
#define	STDIN_NEEDED_ACK	68
#define	STDIN_MODE_ACK		69


/*
** Target to host definitions
*/

#define RESET_ACK     32
#define CONFIG        33
#define STATUS        34
#define READ_ACK      35
#define WRITE_ACK     36
#define BKPT_SET_ACK  37
#define BKPT_RM_ACK   38
#define BKPT_STAT_ACK 39
#define COPY_ACK      40
#define FILL_ACK      41
#define INIT_ACK      42
#define HALT          43

#define ERROR         63

#define HIF_CALL      96
#define CHANNEL0_ACK  97
#define CHANNEL1      98
#define CHANNEL2      99
#define	STDIN_NEEDED_REQ	100
#define	STDIN_MODE_REQ		101

/*
** Endian conversion definitions
*/

#define INCOMING_MSG  0
#define OUTGOING_MSG  1


#ifdef	MSDOS
#define	PARAMS(x)	x
#else
#define	PARAMS(x)	()
#endif

/* A "generic" message */
struct generic_msg_t {
          INT32    code;  /* generic */
          INT32    length;
          BYTE     byte;
          };


/* A "generic" message (with an INT32 array) */
struct generic_int32_msg_t {
          INT32    code;  /* generic */
          INT32    length;
          INT32    int32;
          };


/*
** Host to target messages and routines that build them
*/

struct reset_msg_t {
          INT32    code;  /* 0 */
          INT32    length;
          };

struct config_req_msg_t {
          INT32    code;  /* 1 */
          INT32    length;
          };

struct status_req_msg_t {
          INT32    code;  /* 2 */
          INT32    length;
          };

struct read_req_msg_t {
          INT32    code;  /* 3 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    count;
          INT32    size;
          };

struct write_req_msg_t {
          INT32    code;  /* 4 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    count;
          INT32    size;
          BYTE     data;
          };

struct write_r_msg_t {
          INT32    code;  /* 4 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    byte_count;
          INT32    data;
          };


struct bkpt_set_msg_t {
          INT32    code;  /* 5 */
          INT32    length;
          INT32    memory_space;
          ADDR32   bkpt_addr;
          INT32    pass_count;
          INT32    bkpt_type;
          };

struct bkpt_rm_msg_t {
          INT32    code;  /* 6 */
          INT32    length;
          INT32    memory_space;
          ADDR32   bkpt_addr;
          };

struct bkpt_stat_msg_t {
          INT32    code;  /* 7 */
          INT32    length;
          INT32    memory_space;
          ADDR32   bkpt_addr;
          };

struct copy_msg_t {
          INT32    code;  /* 8 */
          INT32    length;
          INT32    source_space;
          ADDR32   source_addr;
          INT32    dest_space;
          ADDR32   dest_addr;
          INT32    count;
          INT32    size;
          };

struct fill_msg_t {
          INT32    code;  /* 9 */
          INT32    length;
          INT32    memory_space;
          ADDR32   start_addr;
          INT32    fill_count;
          INT32    byte_count;
          BYTE     fill_data;
          };

struct init_msg_t {
          INT32    code;  /* 10 */
          INT32    length;
          ADDR32   text_start;
          ADDR32   text_end;
          ADDR32   data_start;
          ADDR32   data_end;
          ADDR32   entry_point;
          INT32    mem_stack_size;
          INT32    reg_stack_size;
          ADDR32   arg_start;
          INT32    os_control;
	  ADDR32   highmem;   
          };

struct go_msg_t {
          INT32    code;  /* 11 */
          INT32    length;
          };

struct step_msg_t {
          INT32    code;  /* 12 */
          INT32    length;
          INT32    count;
          };

struct break_msg_t {
          INT32    code;  /* 13 */
          INT32    length;
          };

struct hif_call_rtn_msg_t {
          INT32    code;  /* 64 */
          INT32    length;
          INT32    service_number;
          INT32    gr121;
          INT32    gr96;
          INT32    gr97;
          };

struct channel0_msg_t {
          INT32    code;  /* 65 */
          INT32    length;
          BYTE     data;
          };

struct channel1_ack_msg_t {
          INT32    code;  /* 66 */
          INT32    length;
	  INT32	   gr96; 
          };

struct channel2_ack_msg_t {
          INT32    code;  /* 67 */
          INT32    length;
	  INT32	   gr96; 
          };

struct stdin_needed_ack_msg_t {
          INT32    code;  /* 68 */
          INT32    length;
	  BYTE	   data;
          };

struct stdin_mode_ack_msg_t {
          INT32    code;  /* 69 */
          INT32    length;
	  INT32	   mode;
          };

/*
** Target to host messages
*/


struct reset_ack_msg_t {
          INT32    code;  /* 32 */
          INT32    length;
          };


struct config_msg_t {
          INT32    code;  /* 33 */
          INT32    length;
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
          INT32    os_version;
          };


struct status_msg_t {
          INT32    code;  /* 34 */
          INT32    length;
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


struct read_ack_msg_t {
          INT32    code;  /* 35 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    byte_count;
          BYTE     data;
          };

struct read_r_ack_msg_t {
          INT32    code;  /* 35 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    byte_count;
          INT32    data;
          };


struct write_ack_msg_t {
          INT32    code;  /* 36 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    byte_count;
          };


struct bkpt_set_ack_msg_t {
          INT32    code;  /* 37 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          INT32    pass_count;
          INT32    bkpt_type;
          };


struct bkpt_rm_ack_msg_t {
          INT32    code;  /* 38 */
          INT32    length;
          INT32    memory_space;
          ADDR32   address;
          };


struct bkpt_stat_ack_msg_t {
          INT32    code;  /* 39 */
          INT32    length; 
          INT32    memory_space;
          ADDR32   address;
          INT32    pass_count;
          INT32    bkpt_type;
          };


struct copy_ack_msg_t {
          INT32    code;  /* 40 */
          INT32    length;
          INT32    source_space;
          ADDR32   source_addr;
          INT32    dest_space;
          ADDR32   dest_addr;
          INT32    byte_count;
          };


struct fill_ack_msg_t {
          INT32    code;  /* 41 */
          INT32    length;
          INT32    memory_space;
          ADDR32   start_addr;
          INT32    fill_count;
          INT32    byte_count;
          };


struct init_ack_msg_t {
          INT32    code;  /* 42 */
          INT32    length;
          };


struct halt_msg_t {
          INT32    code;  /* 43 */
          INT32    length;
          INT32    memory_space;
          ADDR32   pc0;
          ADDR32   pc1;
          INT32    trap_number;
          };


struct error_msg_t {
          INT32    code;  /* 63 */
          INT32    length;
          INT32    error_code;
          INT32    memory_space;
          ADDR32   address;
          };


struct hif_call_msg_t {
          INT32    code;  /* 96 */
          INT32    length;
          INT32    service_number;
          INT32    lr2;
          INT32    lr3;
          INT32    lr4;
          };


struct channel0_ack_msg_t {
          INT32    code;  /* 97 */
          INT32    length;
          };


struct channel1_msg_t {
          INT32    code;  /* 98 */
          INT32    length;
          BYTE     data;
          };

struct channel2_msg_t {
          INT32    code;  /* 99 */
          INT32    length;
          BYTE     data;
          };

struct stdin_needed_msg_t {
          INT32    code;  /* 100 */
          INT32    length;
          INT32    nbytes;
          };


struct stdin_mode_msg_t {
          INT32    code;  /* 101 */
          INT32    length;
          INT32    mode;
          };


/*
** Union all of the message types together
*/

union msg_t {
         struct generic_msg_t        generic_msg;
         struct generic_int32_msg_t  generic_int32_msg;

         struct reset_msg_t          reset_msg;
         struct config_req_msg_t     config_req_msg;
         struct status_req_msg_t     status_req_msg;
         struct read_req_msg_t       read_req_msg;
         struct write_req_msg_t      write_req_msg;
         struct write_r_msg_t        write_r_msg;
         struct bkpt_set_msg_t       bkpt_set_msg;
         struct bkpt_rm_msg_t        bkpt_rm_msg;
         struct bkpt_stat_msg_t      bkpt_stat_msg;
         struct copy_msg_t           copy_msg;
         struct fill_msg_t           fill_msg;
         struct init_msg_t           init_msg;
         struct go_msg_t             go_msg;
         struct step_msg_t           step_msg;
         struct break_msg_t          break_msg;

         struct hif_call_rtn_msg_t   hif_call_rtn_msg;
         struct channel0_msg_t       channel0_msg;
         struct channel1_ack_msg_t   channel1_ack_msg;
         struct channel2_ack_msg_t   channel2_ack_msg;
	 struct	stdin_needed_ack_msg_t	stdin_needed_ack_msg;
	 struct	stdin_mode_ack_msg_t	stdin_mode_ack_msg;

         struct reset_ack_msg_t      reset_ack_msg;
         struct config_msg_t         config_msg;
         struct status_msg_t         status_msg;
         struct read_ack_msg_t       read_ack_msg;
         struct read_r_ack_msg_t     read_r_ack_msg;
         struct write_ack_msg_t      write_ack_msg;
         struct bkpt_set_ack_msg_t   bkpt_set_ack_msg;
         struct bkpt_rm_ack_msg_t    bkpt_rm_ack_msg;
         struct bkpt_stat_ack_msg_t  bkpt_stat_ack_msg;
         struct copy_ack_msg_t       copy_ack_msg;
         struct fill_ack_msg_t       fill_ack_msg;
         struct init_ack_msg_t       init_ack_msg;
         struct halt_msg_t           halt_msg;

         struct error_msg_t          error_msg;

         struct hif_call_msg_t       hif_call_msg;
         struct channel0_ack_msg_t   channel0_ack_msg;
         struct channel1_msg_t       channel1_msg;
         struct channel2_msg_t       channel2_msg;
	 struct	stdin_needed_msg_t   stdin_needed_msg;
	 struct stdin_mode_msg_t     stdin_mode_msg;
         };

/*
** This macro is used to get the size of a message data
** structure.  The divide then multiply by the sizeof(INT32)
** gets rid of alignment problems which would cause sizeof()
** to return an incorect result.
*/

#define MSG_LENGTH(x)  (((sizeof(x) / sizeof(INT32)) *\
                       sizeof(INT32)) - (2 * sizeof(INT32)))

/* Functions to initialize, send, and receive messages */

INT32   msg_length PARAMS((INT32 code));

INT32	Mini_msg_init PARAMS((char *targname));

int	Mini_alloc_msgbuf PARAMS((int size));

void	Mini_msg_exit PARAMS((void));

INT32	Mini_msg_send PARAMS((void));

INT32	Mini_msg_recv PARAMS((INT32 RecvMode));

INT32	Mini_init_comm PARAMS((void));

INT32	Mini_reset_comm PARAMS((void));

INT32	Mini_exit_comm PARAMS((void));

void	Mini_go_target PARAMS((void));

INT32   Mini_write_memory PARAMS((INT32 m_space, 
				  ADDR32 address, 
				  INT32 byte_count, 
				  BYTE *buffer));

INT32   Mini_read_memory PARAMS((INT32 m_space, 
				  ADDR32 address, 
				  INT32 byte_count, 
				  BYTE *buffer));

/* Function to build specific Minimon messages in "buffer" */

void	Mini_build_reset_msg PARAMS((void));

void	Mini_build_config_req_msg PARAMS((void));

void	Mini_build_status_req_msg PARAMS((void));

void	Mini_build_read_req_msg PARAMS((INT32 memory_space,
					ADDR32 address,
					INT32 count,
					INT32 size));

void	Mini_build_write_req_msg PARAMS((INT32 memory_space,
					 ADDR32 address,
					 INT32 count,
					 INT32 size,
					 BYTE  *data));

void	Mini_build_bkpt_set_msg PARAMS((INT32 memory_space,
                                        ADDR32 bkpt_addr,
					INT32 pass_count,
					INT32 bkpt_type));

void	Mini_build_bkpt_rm_msg PARAMS((INT32 memory_space,
				       ADDR32 bkpt_addr));

void	Mini_build_bkpt_stat_msg PARAMS((INT32 memory_space,
					 ADDR32 bkpt_addr));

void	Mini_build_copy_msg PARAMS((INT32 source_space,
				    ADDR32 source_addr,
				    INT32 dest_space,
				    ADDR32 dest_addr,
				    INT32 count,
				    INT32 size));

void	Mini_build_fill_msg PARAMS((INT32 memory_space,
                                    ADDR32 start_addr,
				    INT32 fill_count,
				    INT32 byte_count,
				    BYTE *fill_data));

void	Mini_build_init_msg PARAMS((ADDR32 text_start,
				    ADDR32 text_end,
				    ADDR32 data_start,
				    ADDR32 data_end,
				    ADDR32 entry_point,
				    INT32 m_stack,
				    INT32 r_stack,
				    ADDR32 highmem,
				    ADDR32 arg_start,
				    INT32 os_control));

void	Mini_build_go_msg PARAMS((void));

void	Mini_build_step_msg PARAMS((INT32 count));

void	Mini_build_break_msg PARAMS((void));

void	Mini_build_hif_rtn_msg PARAMS((INT32 serv_num,
					    INT32 gr121,
					    INT32 gr96,
					    INT32 gr97));

void	Mini_build_channel0_msg PARAMS((BYTE *data, INT32 count));

void	Mini_build_channel1_ack_msg PARAMS((INT32 gr96));

void	Mini_build_channel2_ack_msg PARAMS((INT32 gr96));

void	Mini_build_stdin_needed_ack_msg PARAMS((UINT32 count,BYTE *data));

void	Mini_build_stdin_mode_ack_msg PARAMS((INT32 mode));

/* Functions to unpack/decipher the target to host messages */

void	Mini_unpack_reset_ack_msg PARAMS((void));

void	Mini_unpack_config_msg PARAMS((TIP_TARGET_CONFIG *target_config));

void	Mini_unpack_status_msg PARAMS((TIP_TARGET_STATUS *target_status));

void	Mini_unpack_read_ack_msg PARAMS((INT32 *mspace, 
					 ADDR32	*address,
					 INT32	*byte_count,
					 BYTE *buffer));

void	Mini_unpack_write_ack_msg PARAMS((INT32	*mspace,
					  ADDR32  *address,
					  INT32   *byte_count));

void	Mini_unpack_bkpt_set_ack_msg PARAMS((INT32  *mspace,
					     ADDR32  *address,
					     INT32  *pass_count,
					     INT32  *bkpt_type));

void	Mini_unpack_bkpt_rm_ack_msg PARAMS((INT32  *memory_space,
					ADDR32 *address));

void	Mini_unpack_bkpt_stat_ack_msg PARAMS((INT32  *mspace,
					      ADDR32 *address,
					      INT32 *pass_count,
					      INT32 *bkpt_type));

void	Mini_unpack_copy_ack_msg PARAMS((INT32  *srcspace,
					 ADDR32  *srcaddr,
					 INT32   *dstspace,
					 ADDR32  *dstaddr,
					 INT32   *byte_count));

void	Mini_unpack_fill_ack_msg PARAMS((INT32  *mspace,
					 ADDR32 *startaddr,
					 INT32  *fillcount,
					 INT32  *bytecount));

void	Mini_unpack_init_ack_msg PARAMS((void));

void	Mini_unpack_halt_msg PARAMS((INT32  *mspace,
				     ADDR32 *pc0,
				     ADDR32 *pc1,
				     INT32  *trap_number));

void	Mini_unpack_error_msg PARAMS((INT32  *errcode,
				      INT32  *mspace,
				      ADDR32 *address));

void	Mini_unpack_ch0_ack_msg PARAMS((void));

void	Mini_unpack_channel1_msg PARAMS((BYTE *data,
					 INT32 *len));

void	Mini_unpack_channel2_msg PARAMS((BYTE *data,
					 INT32 *len));

void	Mini_unpack_hif_msg PARAMS((INT32 *gr121,
					INT32 *lr2,
					INT32 *lr3,
					INT32 *lr4));

void	Mini_unpack_stdin_needed_msg PARAMS((INT32 *nbytes));

void	Mini_unpack_stdin_mode_msg PARAMS((INT32 *mode));

void 	CopyMsgFromTarg PARAMS(( union msg_t	*dest));
void 	CopyMsgToTarg PARAMS(( union msg_t	*source));

#define	BUFFER_SIZE	2048

#define	BLOCK	   	1
#define	NONBLOCK	0

#define	MSGRETRY	(INT32) -8

#endif  /* _MESSAGES_H_INCLUDED_ */
