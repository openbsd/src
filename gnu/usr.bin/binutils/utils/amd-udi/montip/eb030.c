static char _[] = "@(#)eb030.c	5.20 93/10/26 09:57:05, Srini, AMD.";
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
 **       This file defines functions which initialize and access the
 **       the EB030 "Lynx" board.  This file is based heavily on the
 **       eb29k.c file.
 *****************************************************************************
 */

#include <memory.h>
#include <string.h>
#include "eb030.h"
#include "types.h"
#include "memspcs.h"
#include "macros.h"
#include "mtip.h"
#include "tdfunc.h"


#include <conio.h>
#include <dos.h>
void  endian_cvt PARAMS((union msg_t *, int));
void  tip_convert32 PARAMS((BYTE *));

/*
** Global variables
*/



/*
** This function is used to initialize the communication
** channel.  This consists of setting the window location
** of the EB030 to the value defined by the values in
** the file eb030.h.
*/

INT32
init_comm_eb030(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   int  result;

   /*** check for existence of the board ***/

   /* Set up memory window location */
   result = outp((unsigned int) PC_port_base,
                 ((int) ((PC_mem_seg >> 10) & 0x1f)));

   /* Set up window base to zero */
   outp ((unsigned int) (PC_port_base+1), (unsigned int) 0);
   outp ((unsigned int) (PC_port_base+2), (unsigned int) 0);

   return(SUCCESS);
   }  /* end init_comm_eb030() */


/*
** This function is used to send a message to the EB030.
** If the message is successfully sent, a zero is
** returned.  If the message was not sendable, a -1
** is returned.
**
** Also note that this function does endian conversion on the
** returned message.  This is necessary because the Am29000
** target will be sending big-endian messages and the PC will
** be expecting little-endian.
*/

INT32
msg_send_eb030(msg_ptr, PC_port_base)
   union  msg_t  *msg_ptr;
INT32	PC_port_base;
   {
   INT32    result;
   INT32  message_size;

#if 0
INT32  semaphore;
   /* Set semaphore (EB030_RECV_BUF_PTR) to zero */
   semaphore = 0;
   result = Mini_write_memory((INT32) D_MEM,
                              (ADDR32) EB030_RECV_BUF_PTR,
                              (INT32) sizeof (INT32),
                              (BYTE *) &semaphore);
#endif

   /* Get size of whole message */
   message_size = (msg_ptr->generic_msg).length + (2 * sizeof(INT32));

   /* Is the size of the message valid? */

   /* Do endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, OUTGOING_MSG);

   /* Send message */
   result = Mini_write_memory((INT32) D_MEM,
                              (ADDR32) EB030_SEND_BUF,
                              (INT32) message_size,
                              (BYTE *) msg_ptr);

   if (result != (INT32) 0)
      return(FAILURE);

   /* Interrupt target (write to EB030 mailbox) */
   result = outp((unsigned int) (PC_port_base+3),
                 (int) 0x00);

   return(SUCCESS);

   }  /* end msg_send_eb030() */




/*
** This function is used to receive a message to the EB030.
** If the message is waiting in the buffer, a zero is
** returned and the buffer pointed to by msg_ptr is filled
** in.  If no message was available, a -1 is returned.
**
** Note that this function does endian conversion on the
** returned message.  This is necessary because the Am29000
** target will be sending big-endian messages and the PC will
** be expecting little-endian.
*/

INT32
msg_recv_eb030(msg_ptr, PC_port_base, Mode)
   union  msg_t  *msg_ptr;
INT32	PC_port_base;
INT32	Mode;
   {
   INT32    result;
   ADDR32 recv_buf_addr;
   INT32  parms_length;
   INT32  header_size;
   INT32  semaphore;
   int	  retval;

   /* Poll EB030 mailbox */
   /* (If mailbox contains 0xff, a message is waiting) */
   retval = inp((unsigned int) (PC_port_base+3));

   /* If no message waiting, return -1 */
   if (retval != 0xff)
      return (-1);

   /* Get receive buffer address */
   result = Mini_read_memory ((INT32) D_MEM,
                              (ADDR32) EB030_RECV_BUF_PTR,
                              (INT32) sizeof (ADDR32),
                              (BYTE *) &recv_buf_addr);

   if (result != (INT32) 0)
      return(FAILURE);

   /* Change endian of recv_buf_addr (if necessary) */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &recv_buf_addr);

   if (recv_buf_addr == (ADDR32) 0) {
      return (FAILURE);
   } else {
   /* Get message header */
   header_size = (INT32) (2 * sizeof(INT32));
   result = Mini_read_memory ((INT32) D_MEM,
                              (ADDR32) recv_buf_addr,
                              (INT32) header_size,
                              (BYTE *) msg_ptr);

   if (result != 0)
      return(FAILURE);

   /* Get rest of message */
   parms_length = (msg_ptr->generic_msg).length;

   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &parms_length);

   /* Is the size of the message valid? */

   result = Mini_read_memory ((INT32) D_MEM,
                               (ADDR32) (recv_buf_addr + header_size),
                               (INT32) parms_length,
                               (BYTE *) &(msg_ptr->generic_msg.byte));
   if (result != 0)
      return(FAILURE);

   /* Do endian conversion (if necessary) */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, INCOMING_MSG);

   /* Write 0xff to EB030 mailbox */
   /* (This tells EB030 that message has been received) */
   retval = outp((unsigned int) (PC_port_base+3), (int) 0xff);

   /* Set semaphore (EB030_RECV_BUF_PTR) to zero */
   semaphore = 0;
   result = Mini_write_memory((INT32) D_MEM,
                              (ADDR32) EB030_RECV_BUF_PTR,
                              (INT32) sizeof (INT32),
                              (BYTE *) &semaphore);

   if (result != 0)
      return(FAILURE);

   return((INT32) msg_ptr->generic_msg.code);
   }
}  /* end msg_recv_eb030() */




/*
** This function is used to reset the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
exit_comm_eb030(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
     return(0);
   }

INT32
reset_comm_eb030(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {

   /* Set up memory window location */
   outp((unsigned int) PC_port_base,
                 ((int) ((PC_mem_seg >> 10) & 0x1f)));

   /* Set up window base to zero */
   outp ((unsigned int) (PC_port_base+1), (unsigned int) 0);
   outp ((unsigned int) (PC_port_base+2), (unsigned int) 0);

   return(0);

   }  /* end reset_comm_eb030() */



/*
** This function is used to "kick" the EB030.  This
** amounts to yanking the *RESET line low.  Code
** will begin execution at ROM address 0.
*/

void
go_eb030(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   int  result;

   /* Toggle the RESET bit in Control Port Register 0 */
   result = outp((unsigned int) PC_port_base,
                 ((int) ((PC_mem_seg >> 10) & 0x1f)));
   result = outp((unsigned int) PC_port_base,
                 ((int) (((PC_mem_seg >> 10) & 0x1f) |
                 EB030_RESET)));

   }  /* end go_eb030() */



/*
** This function is used to write a string of bytes to
** the Am29000 memory on the EB030 board.
**
*/

INT32
write_memory_eb030(memory_space, address, data, byte_count, PC_port_base, PC_mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32	PC_port_base;
   INT32	PC_mem_seg;
   {
   INT32  bytes_in_window;
   INT32  copy_count;

   while (byte_count > 0) {

      /* Write out low order EB030_addr bits */
      outp((unsigned int) (PC_port_base+1), (int) ((address >> 14) & 0xff));
      /* Write out high order EB030_addr bits I-/D-Mem are same */
      outp((unsigned int) (PC_port_base+2), (int) ((address >> 22) & 0x7f));

      bytes_in_window = 0x4000 - (address & 0x3fff);
      copy_count = MIN(byte_count, bytes_in_window);

      (void) memmove ((void *) ((PC_mem_seg << 16) + (address & 0x3fff)),
		      (void *) data,
		      (size_t) copy_count);
#if  0
      (void) movedata((unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (unsigned int) PC_mem_seg,
                      (unsigned int) (address & 0x3fff), 
                      (int) copy_count);
#endif

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(SUCCESS);

   }  /* End write_memory_eb030() */




/*
** This function is used to read a string of bytes from
** the Am29000 memory on the EB030 board.   A zero is
** returned if the data is read successfully, otherwise
** a -1 is returned.
**
*/

INT32
read_memory_eb030(memory_space, address, data, byte_count, PC_port_base, PC_mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32	PC_port_base;
   INT32	PC_mem_seg;
   {
   INT32  bytes_in_window;
   INT32  copy_count;

   while (byte_count > 0) {

      /* Write out low order EB030_addr bits */
      outp((unsigned int) (PC_port_base+1), (int) ((address >> 14) & 0xff));
      /* Write out high order EB030_addr bits I/D are same */
      outp((unsigned int) (PC_port_base+2), (int) ((address >> 22) & 0x7f));

      bytes_in_window = 0x4000 - (address & 0x3fff);
      copy_count = MIN(byte_count, bytes_in_window);

#if 0
      (void) memmove ((void *) data,
		      (void *) ((PC_mem_seg << 16) + (address & 0x3fff)),
		      (size_t) copy_count);
#endif
      (void) movedata((unsigned int) PC_mem_seg,
                      (unsigned int) (address & 0x3fff), 
                      (unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (int) copy_count);

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(SUCCESS);

   }  /* End read_memory_eb030() */

INT32
fill_memory_eb030()
{
  return(SUCCESS);
}
