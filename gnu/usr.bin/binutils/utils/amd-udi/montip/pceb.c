static char _[] = "@(#)pceb.c	5.18 93/07/30 16:40:31, AMD.";
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
 **       This file defines functions which initialize and access the
 **       the PCEB 29K board.
 **
 *****************************************************************************
 */


#include <stdio.h>
#include <memory.h>
#include "messages.h"
#include "pceb.h"
#include "memspcs.h"
#include "macros.h"
#include "tdfunc.h"
#include "mtip.h"

#include <dos.h>
#include <conio.h>
void  endian_cvt PARAMS((union msg_t *, int));
void  tip_convert32 PARAMS((BYTE *));


/*
** This function is used to initialize the communication
** channel.  This consists of setting the window location
** of the PCEB to the value defined by the values in
** the file PCEB.h.
*/

/*ARGSUSED*/
INT32
init_comm_pceb(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   
   /*** check for existence of the board ***/

   /* Set up PCCNF and reset processor */
   outp((unsigned int) (PC_port_base + PCEB_PCCNF_OFFSET),
        ((int) (PC_mem_seg & 0x7000) >> 10));
   outp((unsigned int) (PC_port_base + PCEB_PC229K_OFFSET),
        (int) (PCEB_LB_END | PCEB_WINENA | PCEB_S_HALT));

   return(0);
   }


/*
** This function is used to send a message to the PCEB.
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
msg_send_pceb(msg_ptr, PC_port_base)
   union  msg_t  *msg_ptr;
   INT32	PC_port_base;
   {
   INT32    result;
   int    pc229k;
   INT32  message_size;
   INT32	semaphore;
   INT32	result3;

         /* Set semaphore (PCEB_RECV_BUF_PTR) to zero */
	   semaphore = (INT32) 0;
           result3 = Mini_write_memory ((INT32)   D_MEM,
                                      (ADDR32) PCEB_RECV_BUF_PTR,
                                      (INT32)  sizeof(INT32),
                                      (BYTE *) &semaphore);
   /* Get size of whole message */
   message_size = (msg_ptr->generic_msg).length + (2 * sizeof(INT32));

   /* Do endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, OUTGOING_MSG);

   /* Send message */
   result = Mini_write_memory ((INT32)  D_MEM,
                              (ADDR32) PCEB_SEND_BUF,
                              (INT32)  message_size,
                              (BYTE *) msg_ptr);

   /* Interrupt target (write to pceb mailbox) */
   pc229k = (PCEB_P_REQ | PCEB_WINENA | PCEB_LB_END | PCEB_S_NORMAL);
   outp((unsigned int) (PC_port_base + PCEB_PC229K_OFFSET),
        (int) pc229k);

   /* Did everything go ok? */
   if (result != 0)
      return(-1);
      else
         return(0);

   }  /* end msg_send_pceb() */




/*
** This function is used to receive a message to the PCEB.
** If the message is waiting in the buffer, the message Code is
** returned and the buffer pointed to by msg_ptr is filled
** in.  If no message was available, a -1 is returned.
**
** Note that this function does endian conversion on the
** returned message.  This is necessary because the Am29000
** target will be sending big-endian messages and the PC will
** be expecting little-endian.
*/

INT32
msg_recv_pceb(msg_ptr, PC_port_base, Mode)
   union  msg_t  *msg_ptr;
   INT32  PC_port_base;
   INT32	Mode;
   {
   INT32    result1;
   INT32    result2;
   INT32    result3;
   ADDR32 recv_buf_addr;
   INT32  parms_length;
   INT32  header_size;
   INT32  semaphore;

   /* Get receive buffer address */
   result1 = Mini_read_memory ((INT32)  D_MEM,
                              (ADDR32) PCEB_RECV_BUF_PTR,
                              (INT32)  sizeof(ADDR32),
                              (BYTE *) &recv_buf_addr);

   /* Change endian of recv_buf_addr */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &recv_buf_addr);

   /* Return if there is no message */
   if (recv_buf_addr == 0) {
      return(-1);
      } else {
         /* Get message header */
         header_size = (INT32) (2 * sizeof(INT32));
         result1 = Mini_read_memory ((INT32)  D_MEM,
                                    (ADDR32) recv_buf_addr,
                                    (INT32)  header_size,
                                    (BYTE *) msg_ptr);

         /* Get rest of message */
         parms_length = (msg_ptr->generic_msg).length;
         if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
            tip_convert32((BYTE *) &parms_length);
         result2 = Mini_read_memory ((INT32)  D_MEM,
                                    (ADDR32) (recv_buf_addr + header_size),
                                    (INT32)  parms_length,
                                    (BYTE *) &(msg_ptr->generic_msg.byte));

         /* Do endian conversion */
         if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
            endian_cvt(msg_ptr, INCOMING_MSG);

         /* Set semaphore (PCEB_RECV_BUF_PTR) to zero */
	   semaphore = (INT32) 0;
           result3 = Mini_write_memory ((INT32)   D_MEM,
                                      (ADDR32) PCEB_RECV_BUF_PTR,
                                      (INT32)  sizeof(INT32),
                                      (BYTE *) &semaphore);
      }

   /* Did everything go ok? */
   if ((result1 != (INT32) 0) ||
       (result2 != (INT32) 0) ||
       (result3 != (INT32) 0))
         return(-1);
      else
         return(msg_ptr->generic_msg.code);

   }  /* end msg_recv_pceb() */




/*
** This function is used to close the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
exit_comm_pceb(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
     return (0);
   }

INT32
reset_comm_pceb(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {

   return(0);

   }  /* end reset_comm_pceb() */



/*
** This function is used to "kick" the PCEB.  This
** amounts to yanking the *RESET line low.  Code
** will begin execution at ROM address 0.
*/

void
go_pceb(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   int  setup;

   /* Reset processor */
   setup = (PCEB_LB_END | PCEB_WINENA);

   outp((unsigned int) (PC_port_base + PCEB_PC229K_OFFSET),
        (int) (setup | PCEB_S_RESET | PCEB_S_HALT));

   outp((unsigned int) (PC_port_base + PCEB_PC229K_OFFSET),
        (int) (setup | PCEB_S_RESET | PCEB_S_NORMAL));

   outp((unsigned int) (PC_port_base + PCEB_PC229K_OFFSET),
        (int ) (setup | PCEB_S_NORMAL));

   }  /* end go_pceb() */






/*
** This function is used to write a string of bytes to
** the Am29000 memory on the PCEB board.
**
** For more information on the PCEB interface, see
** Chapter 5 of the "PCEB User's Manual".
**
** Note:  This function aligns all 16 1K byte windows to make
** a single 16K byte window on a 1K boundary.
*/

INT32
write_memory_pceb(memory_space, address, data, byte_count, PC_port_base,
		  PC_mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32	PC_port_base;
   INT32	PC_mem_seg;
   {
   INT32  i;
   ADDR32 temp_address;
   INT32  bytes_in_window;
   INT32  copy_count;

   while (byte_count > 0) {

      /* Set up a single, contiguous 16K window (on a 1K boundary) */
      temp_address = address;
      for (i=0; i<16; i=i+1) {
         /* Write out low PCEB addr bits */
         outp((unsigned int) (PC_port_base+(INT32) (2*i)),
              (int) ((temp_address >> 10) & 0xff));

         /* Write out high PCEB addr bits */
         outp((unsigned int) (PC_port_base+(INT32) (2*i)+(INT32) 1),
              (int) ((temp_address >> 18) & 0x1f));
         temp_address = temp_address + (ADDR32) 0x400;
         }  /* end for */

      bytes_in_window = (INT32) 0x4000 - (address & 0x3ff);
      copy_count = (byte_count < bytes_in_window) ? byte_count : bytes_in_window;

      (void) movedata((unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (unsigned int) PC_mem_seg,
                      (unsigned int) (address & 0x3ff),
                      (int) copy_count);

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(0);

   }  /* End write_memory_pceb() */




/*
** This function is used to read a string of bytes from
** the Am29000 memory on the PCEB board.   A zero is
** returned if the data is read successfully, otherwise
** a -1 is returned.
**
** For more information on the PCEB interface, see
** Chapter 5 of the "PCEB User's Manual".
**
** Note:  This function aligns all 16 1K byte windows to make
** a single 16K byte window on a 1K boundary.
*/

INT32
read_memory_pceb(memory_space, address, data, byte_count, PC_port_base,
		 PC_mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32	PC_port_base;
   INT32	PC_mem_seg;
   {
   INT32  i;
   ADDR32 temp_address;
   INT32  bytes_in_window;
   INT32  copy_count;

   while (byte_count > 0) {

      /* Set up a single, contiguous 16K window (on a 1K boundary) */
      temp_address = address;
      for (i=0; i<16; i=i+1) {
         /* Write out low PCEB addr bits */
         outp((unsigned int) (PC_port_base+(2*i)),
              (int) ((temp_address >> 10) & 0xff));

         /* Write out high PCEB addr bits */
         outp((unsigned int) (PC_port_base+(2*i)+(INT32) 1),
              (int) ((temp_address >> 18) & 0x1f));
         temp_address = temp_address + (ADDR32) 0x400;
         }  /* end for */

      bytes_in_window = (INT32) 0x4000 - (address & 0x3ff);
      copy_count = (byte_count < bytes_in_window) ? byte_count : bytes_in_window;

      (void) movedata((unsigned int) PC_mem_seg,
                      (unsigned int) (address & 0x3ff),
                      (unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (int) copy_count);

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(0);

   }  /* End read_memory_pceb() */

INT32
fill_memory_pceb()
{
  return(0);
}
