static char _[] = "@(#)parallel.c	1.4 93/09/08 14:14:32, Srini, AMD.";
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
 ****************************************************************************
 * Engineer:  Srini Subramanian.
 ****************************************************************************
 */
#include <bios.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>

#include "types.h"
#include "memspcs.h"
#include "messages.h"
#include "mtip.h"
#include "tdfunc.h"

void   endian_cvt PARAMS((union msg_t *, int));

extern	FILE	*MsgFile;	/* for logging error retries */

unsigned _bios_printer(unsigned service, unsigned printer, unsigned data);


INT32 par_write( char	*buffer, INT32	length);

static	unsigned	portID=0;

#define	LPT1	0
#define	LPT2	1

#define CHECKSUM_FAIL -1

INT32
init_parport(portname)
char	*portname;
{
  unsigned status;

  if (strncmp(portname, "lpt1", 4) == 0)  {
     status = _bios_printer( _PRINTER_INIT, LPT1, 0);
     portID = LPT1;
  } else if (strncmp(portname, "lpt2", 4) == 0) {
     status = _bios_printer( _PRINTER_INIT, LPT2, 0);
     portID = LPT2;
  }
#if 0
  if (status != 0x90) {
    printf("parallel port status 0x%.4x\n", status);
    return ((INT32) -1);
  } else {
    return ((INT32) 0);
  }
#endif
    return ((INT32) 0);
}


INT32
msg_send_parport(msg_ptr, port_base)
union  msg_t  *msg_ptr;
INT32  port_base;
{
   INT32 result, i, ack, comm_err; 
   UINT32 checksum;
   unsigned int		timeout;
   INT32 	Rx_ack[2];

   INT32 header_size = (2 * sizeof(INT32));

   BYTE  *bfr_ptr = (BYTE *) msg_ptr;

   /* Save length before doing endian conversion */
   INT32 length = msg_ptr->generic_msg.length;
   INT32 total_length;
	
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
	comm_err = (INT32) 0;

	/* send msg */ 
        result = par_write((char *)bfr_ptr, total_length+4 /* +4 */);
	if (result != (INT32) 0)
	    return((INT32) FAILURE);

	/* get ack */
	timeout = 0;
	result = (INT32) -1;
	comm_err = (INT32) 0;
	while ((timeout < 600) && (result == (INT32) -1) 
					  && (comm_err == (INT32) 0)) {
	/* Poll for user interrupt */
	   timeout=timeout+1;
           result = recv_bfr_serial((BYTE *) Rx_ack, (2 * sizeof(INT32)), 
					BLOCK, port_base, &comm_err);
	}

	if (comm_err != (INT32) 0) {
	     reset_comm_serial((INT32) -1, (INT32) -1);
	     return ((INT32) MSGRETRY);
	}
	/* check if timed out */
	if (timeout >= 10000) {
	 if (MsgFile) {
	   fprintf(MsgFile,"Timed out before ACK received. Reset comm. timeout=%ld\n",timeout);
     	   fflush(MsgFile);
	  }
	  (void) reset_comm_serial((INT32) 0, (INT32) 0);
	  return ((INT32) MSGRETRY);
	}

	ack = (INT32) Rx_ack[1];

	/* endian convert Ack */
    	if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
       			        tip_convert32((BYTE *) &ack);

	if (ack != CHECKSUM_FAIL) { 
		return(0);		/* successful send */
		}
	else {
  		if (MsgFile) {	/* log the error */
     	 		fprintf(MsgFile, 
			  "\n** Checksum: Nack Received, Resending.\n");
     			fflush(MsgFile);
  			};
		}   

   return ((INT32) FAILURE);

}

INT32
par_write(buffer, length)
char	*buffer;
INT32 		length;
{

 unsigned 	status;

 for ( ; length > (INT32) 0; length=length-1)
 {
   status = _bios_printer(_PRINTER_WRITE, portID, (unsigned) *buffer);
   /* printf("status 0x%.4x \n", status); */
   buffer++;
 }
 return ((INT32) 0);
}
