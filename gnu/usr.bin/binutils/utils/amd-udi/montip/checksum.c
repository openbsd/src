static char _[] = "@(#)checksum.c	5.25 93/10/27 15:11:54, Srini, AMD.";
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
 **
 **       This file contains code for intercepting failed messages
 **	 due to serial transmission errors.  The code logically
 **	 resides between the messages system and the character
 ** 	 based serial driver.  Messages with invalid checksums, or
 ** 	 other communication errors are retried.
 *****************************************************************************
 */

#include <stdio.h>
#include <string.h>
#include "types.h"
#include "memspcs.h"
#include "messages.h"
#include "mtip.h"
#include "tdfunc.h"

#ifdef MSDOS
#include <conio.h>
#endif

#define ACK_HDR_CODE -1
#define CHECKSUM_PASS 0
#define CHECKSUM_FAIL -1

extern	int	MessageRetries;
extern	unsigned int	TimeOut;
extern	unsigned int	BlockCount;
#ifdef __hpux
static volatile int	bcount;
#else
static int	bcount;
#endif

extern	int	use_parport;

#ifdef MSDOS
INT32	par_write PARAMS ((char *, INT32));
#endif

void   endian_cvt PARAMS((union msg_t *, int));
void   send_nack PARAMS((INT32 port_base));

extern	FILE	*MsgFile;	/* for logging error retries */

struct	ack_msg_t {
  INT32		code;
  INT32		passfail;
};
union  ack_msg_buf_t {
   struct ack_msg_t  ack_msg;
   unsigned char     buf[(2*sizeof(INT32))];
};


INT32
msg_send_serial(msg_ptr, port_base)
   union  msg_t  *msg_ptr;
   INT32  port_base;
   {
   INT32 result, i, ack, comm_err; 
   UINT32 checksum;
   int		retries;
   unsigned int		timeout;
   INT32 	Rx_ack[2];

   INT32 header_size = (2 * sizeof(INT32));

   BYTE  *bfr_ptr = (BYTE *) msg_ptr;

   /* Save length before doing endian conversion */
   INT32 length = msg_ptr->generic_msg.length;
   INT32 total_length;
	
   /*
    * MiniMON29K release 2.1 has new Communications Interface module
    * which does not expect the checksum to be aligned on a word 
    * boundary. It expects the checksum to immediately follow the
    * end of the message body.
    * The old handler aligned the checksum on the next word boundar after
    * the message body, but did _not_ update the message length field.
    * That caused problems when one end of the Communications gets
    * changed.
    */
   if (((tip_target_config.version >> 24) & 0xf) > 5) { /* new comm handler */
   } else { /* old comm handler */
      /* round length up to even word */
      if ((length & 3) != 0) {	/* round up to word boundary */
   	   length = length + 3;
   	   length = length & 0xfffffffc;
      };
   }

   total_length = header_size + length;

   /* Endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, OUTGOING_MSG);

   /* calc checksum for msg */
   checksum = 0;    
   for (i=0; i < total_length; i++)
     checksum = checksum + bfr_ptr[i];

   /* Append checksum to the end of the message. Do not update the
    * "length" field of the message header.
    */
   bfr_ptr[total_length] = (BYTE) ((checksum >> 24) & 0xff);
   bfr_ptr[total_length+1] = (BYTE) ((checksum >> 16) & 0xff);
   bfr_ptr[total_length+2] = (BYTE) ((checksum >> 8) & 0xff);
   bfr_ptr[total_length+3] = (BYTE) ((checksum >> 0) & 0xff);
   /* send msg */
   retries = 0;
   do  {
	retries = retries + 1;
	comm_err = (INT32) 0;

	/* send msg */ 
        result = send_bfr_serial(bfr_ptr, total_length+4, /* 4 for checksum*/
					port_base, &comm_err);
	if (comm_err != (INT32) 0) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	  return ((INT32) MSGRETRY);
	}
	if (result != (INT32) 0)
	    return((INT32) FAILURE);

	/* get ack */
	timeout = 0;
	result = (INT32) -1;
	comm_err = (INT32) 0;
	while ((timeout < TimeOut) && (result == (INT32) -1) 
					  && (comm_err == (INT32) 0)) {
	/* Poll for user interrupt */
	   SIGINT_POLL
	   timeout=timeout+1;
           result = recv_bfr_serial((BYTE *) Rx_ack, (2 * sizeof(INT32)), 
					BLOCK, port_base, &comm_err);
#ifndef MSDOS
	   /* printf("ack wait timeout=0x%lx\n", timeout); */
	   if (result == (INT32) -1)
	     for (bcount = 0; bcount < BlockCount; bcount++);
#endif
	}

	if (comm_err != (INT32) 0) {
	     reset_comm_serial((INT32) -1, (INT32) -1);
	     return ((INT32) MSGRETRY);
	}
	/* Poll for user interrupt */
	   SIGINT_POLL
	/* check if timed out */
	if (timeout >= TimeOut) {
	 if (MsgFile) {
	   fprintf(MsgFile,"Timed out before ACK received. Reset comm. retries=%d timeout=%ld\n",retries, timeout);
     	   fflush(MsgFile);
	  }
	  (void) reset_comm_serial((INT32) 0, (INT32) 0);
	  continue;
	}

	ack = (INT32) Rx_ack[1];

	/* endian convert Ack */
    	if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
       			        tip_convert32((BYTE *) &ack);

	if (Rx_ack[0] == (INT32) 0xFFFFFFFF && ack != CHECKSUM_FAIL) { 
		return(0);		/* successful send */
		}
	else {
	  (void) reset_comm_serial((INT32) 0, (INT32) 0);
  		if (MsgFile) {	/* log the error */
     	 		fprintf(MsgFile, 
			  "\n** Checksum: Nack Received, Resending.\n");
     			fflush(MsgFile);
  			};
		}   

   } while ( retries < MessageRetries);
   return ((INT32) FAILURE);
}

INT32
msg_recv_serial(msg_ptr, port_base, Mode)
   union  msg_t  *msg_ptr;
   INT32  port_base;
   INT32  Mode;	/* Block or NonBlock */
   {
   union  ack_msg_buf_t  AckMsg;
   UINT32 checksum_calc, checksum_recv;
   INT32 i, result;
   INT32 comm_err;
   INT32 ack_hdr;
   BYTE  *bfr_ptr;
   INT32 header_size;
   INT32 length, total_length;

again:
	/* Poll for user interrupt */
	   SIGINT_POLL

   comm_err = (INT32) 0;
   ack_hdr = (INT32) ACK_HDR_CODE;
   bfr_ptr = (BYTE *) msg_ptr;
   header_size = (2 * sizeof(INT32));

   /* recv header - if available */ 
   if (Mode == NONBLOCK) {
#ifndef MSDOS
	     for (bcount = 0; bcount < BlockCount; bcount++);
#endif
     result = recv_bfr_serial(bfr_ptr, header_size, Mode,
					port_base, &comm_err);
	      /* printf("nbread: result = 0x%lx Mode=0x%lx\n", result, Mode); */
   } else {
	     /* printf("bread: header_size = %d Mode=0x%lx\n", header_size, Mode); */
     result = recv_bfr_serial(bfr_ptr, header_size, Mode,
					port_base, &comm_err);
     if (result == (INT32) -1) {
#ifndef MSDOS
	     for (bcount = 0; bcount < BlockCount; bcount++);
#endif
	     goto again;
     }
   }
   if (comm_err != (INT32) 0) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	  send_nack(port_base);
	  goto again;
   }
   if (result != (INT32) 0)
	    return((INT32) FAILURE);

	/* Poll for user interrupt */
	   SIGINT_POLL
   /*
    * Before computing the length here, we should make sure that we have
    * received a valid (defined) MiniMON29K message by checking the
    * Message Code field. Otherwise, a lousy stream of bytes could send this
    * to a toss waiting for an unknown number of bytes.
    * But we hope none of those things would happen here!
    */
   result = msg_ptr->generic_msg.code;
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
        	tip_convert32((BYTE *) &result);
   if ((result < (INT32) 0) || (result > 101)) {
	(void) reset_comm_serial ((INT32) -1, (INT32) -1);
	send_nack(port_base);
	goto again;	/* retry */
   }
   /* Message header received.  Save message length. */
   length = msg_ptr->generic_msg.length;
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
        	tip_convert32((BYTE *) &length);

   /*
    * MiniMON29K release 2.1 has new Communications Interface module
    * which does not expect the checksum to be aligned on a word 
    * boundary. It expects the checksum to immediately follow the
    * end of the message body.
    * The old handler aligned the checksum on the next word boundar after
    * the message body, but did _not_ update the message length field.
    * That caused problems when one end of the Communications gets
    * changed.
    */
   if (((tip_target_config.version >> 24) & 0xf) > 5) { /* new comm handler */
   } else { /* old comm handler */
      /* round length up to even word */
      if ((length & 3) != 0) {	
   	   length = length + 3;
   	   length = length & 0xfffffffc;
      }
   }

   /* committed now - recv rest of msg and checksum */
   comm_err = (INT32) 0;
   result = (INT32) 0;
   if (length >= 0) {
   	result = recv_bfr_serial(bfr_ptr + header_size, length+4,/* +4 */
				BLOCK, port_base, &comm_err);
   }

   if (comm_err != (INT32) 0) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	  send_nack(port_base);
	  goto again;	/* retry */
   }
   if (result != (INT32) 0) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	    send_nack(port_base);
	    goto again;	/* retry */
    }


   /* Do endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      	endian_cvt(msg_ptr, INCOMING_MSG);

   /* calc checksum for msg */
   checksum_calc = 0;    
   total_length = header_size + length;
   for (i=0; i < total_length; i++)
      checksum_calc = checksum_calc + ((UINT32) bfr_ptr[i]);

   checksum_recv = (UINT32) 0;
   checksum_recv = (UINT32) (checksum_recv | ((UINT32) bfr_ptr[total_length] << 24));
   checksum_recv = (UINT32) (checksum_recv | ((UINT32) bfr_ptr[total_length+1] << 16));
   checksum_recv = (UINT32) (checksum_recv | ((UINT32) bfr_ptr[total_length+2] << 8));
   checksum_recv = (UINT32) (checksum_recv | ((UINT32) bfr_ptr[total_length+3] << 0));

	/* Poll for user interrupt */
	   SIGINT_POLL
   /* Compare Checksums */
   if (checksum_calc != checksum_recv) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	send_nack(port_base);
	goto	again;	/* retry */
	}

   /* send checksum hdr & ack */
   AckMsg.ack_msg.code = (INT32) ACK_HDR_CODE;
   AckMsg.ack_msg.passfail = CHECKSUM_PASS;
   result = (INT32) 0;
   comm_err = (INT32) 0;
#ifdef MSDOS
   if (use_parport) {
     result = par_write ((char *) AckMsg.buf, 8);
   } else {
     result = send_bfr_serial((BYTE *) AckMsg.buf, 2 * sizeof(INT32),
				port_base, &comm_err);
     if (comm_err != (INT32) 0) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	  send_nack(port_base);
	  goto	again;	/* retry */
     }
     if (result != (INT32) 0)  {
       if (MsgFile) {
         fprintf(MsgFile, "Couldn't send checksum to acknowledge.\n");
         fflush (MsgFile);
       }
       (void) reset_comm_serial((INT32) -1, (INT32) -1);
       send_nack(port_base);
       goto	again;	/* retry */
     }
   }
#else
     result = send_bfr_serial((BYTE *) AckMsg.buf, 2 * sizeof(INT32),
				port_base, &comm_err);
     if (comm_err != (INT32) 0) {
	  (void) reset_comm_serial ((INT32) -1, (INT32) -1);
	  send_nack(port_base);
	  goto	again;	/* retry */
     }
     if (result != (INT32) 0)  {
       if (MsgFile) {
         fprintf(MsgFile, "Couldn't send checksum to acknowledge.\n");
         fflush (MsgFile);
       }
       (void) reset_comm_serial((INT32) -1, (INT32) -1);
       send_nack(port_base);
       goto	again;	/* retry */
     }
#endif

   return(msg_ptr->generic_msg.code);   /* passed */
}

void
SendACK(port_base)
   INT32  port_base;
   {
   union ack_msg_buf_t  AckMsg;
   INT32 result, comm_err;
   INT32 ack_hdr = (INT32) ACK_HDR_CODE;
   INT32 ack = CHECKSUM_FAIL;

   AckMsg.ack_msg.code = (INT32) ACK_HDR_CODE;
   AckMsg.ack_msg.passfail = CHECKSUM_PASS;
   result = (INT32) 0;
   comm_err = (INT32) 0;
#ifdef MSDOS
   if (use_parport) {
     result = par_write((char *) AckMsg.buf, 8);
     return;
   } else {
      result = send_bfr_serial((BYTE *) AckMsg.buf, 2*sizeof(INT32),
				    port_base, &comm_err);
      if ((result != (INT32) 0) || (comm_err != (INT32) 0)) {
        if (MsgFile) {
          fprintf(MsgFile, "Couldn't send ACK to remote.\n");
          fflush (MsgFile);
        }
        return ;
      }
   }
#else
      result = send_bfr_serial((BYTE *) AckMsg.buf, 2*sizeof(INT32),
				    port_base, &comm_err);
      if ((result != (INT32) 0) || (comm_err != (INT32) 0)) {
        if (MsgFile) {
          fprintf(MsgFile, "Couldn't send ACK to remote.\n");
          fflush (MsgFile);
        }
        return ;
      }
#endif
} 

void
send_nack(port_base)
   INT32  port_base;
   {
   union ack_msg_buf_t  NAckMsg;
   INT32 result, comm_err;
   INT32 ack_hdr = (INT32) ACK_HDR_CODE;
   INT32 ack = CHECKSUM_FAIL;

   /* eat up any incoming characters */
   result = reset_comm_serial(port_base, port_base); 	/* reset buffer */

   if (MsgFile) {	/* log the error */
  	fprintf(MsgFile, 
	  "\n** Checksum: Receive failed, sending Nack.\n");
     	fflush(MsgFile);
  	};

	/* Poll for user interrupt */
	   SIGINT_POLL
   NAckMsg.ack_msg.code = (INT32) ACK_HDR_CODE;
   NAckMsg.ack_msg.passfail = CHECKSUM_FAIL;
   result = (INT32) 0;
   comm_err = (INT32) 0;
#ifdef MSDOS
   if (use_parport) {
     result = par_write((char *) NAckMsg.buf, 8);
     return;
   } else {
      result = send_bfr_serial((BYTE *) NAckMsg.buf, 2*sizeof(INT32),
				    port_base, &comm_err);
      if ((result != (INT32) 0) || (comm_err != (INT32) 0)) {
        if (MsgFile) {
          fprintf(MsgFile, "Couldn't send NACK to remote.\n");
          fflush (MsgFile);
        }
        return ;
      }
   }
#else
      result = send_bfr_serial((BYTE *) NAckMsg.buf, 2*sizeof(INT32),
				    port_base, &comm_err);
      if ((result != (INT32) 0) || (comm_err != (INT32) 0)) {
        if (MsgFile) {
          fprintf(MsgFile, "Couldn't send NACK to remote.\n");
          fflush (MsgFile);
        }
        return ;
      }
#endif
} 
