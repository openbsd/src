static char _[]="@(#)lcb29k.c	5.22 93/10/26 09:57:08, Srini, AMD.";
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
 * Engineer: Srini Subramanian.
 ****************************************************************************
 **       This file defines functions which initialize and access the
 **       LCB29K (Low Cost Board 29K) or "squirt" board from YARC.
 **
 ****************************************************************************
 */

#include <stdio.h>
#include <memory.h>
#include "types.h"
#include "lcb29k.h"
#include "memspcs.h"
#include "mtip.h"
#include "tdfunc.h"
#include "macros.h"

#include <conio.h>
#include <dos.h>

void    endian_cvt PARAMS((union msg_t *, int));
void    tip_convert32 PARAMS ((BYTE *));


/*
** This function is used to initialize the communication
** channel.  This consists of setting the window location
** of the LCB29K to the value defined by the values in
** the file lcb29k.h.
*/

INT32
init_comm_lcb29k(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   int  result;
   int  control_reg;

   /*** check for existence of the board ***/

   /* Initialize Control Port Register 0 */
   /* (But don't set LCB29K_RST)         */
   control_reg = (LCB29K_CLRINPC | LCB29K_INTEN | LCB29K_WEN);
   result = outp((unsigned int) PC_port_base,
                 control_reg);

   return(0);
   }  /* end init_comm_lcb29k() */


/*
** This function is used to send a message to the LCB29K.
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
msg_send_lcb29k(msg_ptr, PC_port_base)
   union  msg_t  *msg_ptr;
   INT32	PC_port_base;
   {
   int    result;
   int    control_reg;
   INT32  message_size;


#if 0
   INT32	semaphore;
   /* Set semaphore (LCB29K_RECV_BUF_PTR) to zero */
   semaphore = 0;
   result = (int) Mini_write_memory((INT32)  D_MEM,
                                (ADDR32) LCB29K_RECV_BUF_PTR,
                                (INT32)  sizeof(INT32),
                                (BYTE *) &semaphore);

   if (result != 0)
      return(-1);
#endif
   /* Get size of whole message */
   message_size = (msg_ptr->generic_msg).length + (2 * sizeof(INT32));

   /* Do endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, OUTGOING_MSG);

   /* Send message */
   result = (int) Mini_write_memory((INT32)  D_MEM,
                                (ADDR32) LCB29K_SEND_BUF,
                                (INT32)  message_size,
                                (BYTE *) msg_ptr);

   if (result != 0)
      return(-1);

   /* Interrupt target (write to "LCB29K" mailbox) */
   /* Note:  This sequence of bytes written to the
   **        port of the low cost board should cause
   **        the target to be interrupted.  This
   **        sequence was given to AMD by YARC systems.
   */

/*
   control_reg = (LCB29K_RST);
   result = outp((unsigned int) (PC_port_base),
                 control_reg);

   control_reg = (LCB29K_RST | LCB29K_WEN);
   result = outp((unsigned int) (PC_port_base),
                 control_reg);
*/

   control_reg = (LCB29K_RST | LCB29K_INTEN | LCB29K_WEN);
   result = outp((unsigned int) (PC_port_base),
                 control_reg);

   control_reg = (LCB29K_RST | LCB29K_INTEN | LCB29K_WEN |
                  LCB29K_INT29);
   result = outp((unsigned int) (PC_port_base),
                 control_reg);
/*
   control_reg = (LCB29K_RST | LCB29K_INTEN | LCB29K_WEN |
                  LCB29K_INT29);
   result = outp((unsigned int) (PC_port_base),
                 control_reg);
*/
   return(0);

   }  /* end msg_send_lcb29k() */




/*
** This function is used to receive a message to the LCB29K.
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
msg_recv_lcb29k(msg_ptr, PC_port_base, Mode)
   union  msg_t  *msg_ptr;
   INT32	PC_port_base;
   INT32	Mode;
   {
   int    result;
   ADDR32 recv_buf_addr;
   INT32  parms_length;
   INT32  header_size;
   INT32  semaphore;
   int    control_reg;

   /* Poll LCB29K control register */
   control_reg = inp((unsigned int) PC_port_base);

   /* If LCB29K_INTPC flag set, message ready */
   if ((control_reg & LCB29K_INTPC) != 0) {
     /* Clear LCB29K_INTPC (and don't interrupt 29K) */
     control_reg = ((control_reg & ~(LCB29K_INT29)) | LCB29K_CLRINPC);

     result = outp((unsigned int) (PC_port_base),
                   control_reg);

     control_reg = (control_reg & ~(LCB29K_CLRINPC));

     result = outp((unsigned int) (PC_port_base),
                   control_reg);
   }

   /* Get receive buffer address */
   result = (int) Mini_read_memory((INT32)  D_MEM,
                               (ADDR32) LCB29K_RECV_BUF_PTR,
                               (INT32)  sizeof(ADDR32),
                               (BYTE *) &recv_buf_addr);

   if (result != 0)
      return(-1);

   /* Change endian of recv_buf_addr (if necessary) */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &recv_buf_addr);

   /* If no message waiting, return -1 (This shouldn't happen) */
   if (recv_buf_addr == (ADDR32) 0)
      return (-1);

   /* Get message header */
   header_size = (INT32) (2 * sizeof(INT32));
   result = (int) Mini_read_memory((INT32)  D_MEM,
                               (ADDR32) recv_buf_addr,
                               (INT32)  header_size,
                               (BYTE *) msg_ptr);

   if (result != 0)
      return(-1);

   /* Get rest of message */
   parms_length = (msg_ptr->generic_msg).length;

   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &parms_length);


   result = (int) Mini_read_memory((INT32)  D_MEM,
                               (ADDR32) (recv_buf_addr + header_size),
                               (INT32)  parms_length,
                               (BYTE *) &(msg_ptr->generic_msg.byte));
   if (result != 0)
      return(-1);

   /* Do endian conversion (if necessary) */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, INCOMING_MSG);

   /* Write Clear LCB29K_INPC */
   control_reg = (LCB29K_RST | LCB29K_CLRINPC | LCB29K_INTEN |
                  LCB29K_WEN);
   result = outp((unsigned int) (PC_port_base),
                 control_reg);

   /* Set semaphore (LCB29K_RECV_BUF_PTR) to zero */
   semaphore = 0;
   result = (int) Mini_write_memory((INT32)  D_MEM,
                                (ADDR32) LCB29K_RECV_BUF_PTR,
                                (INT32)  sizeof(INT32),
                                (BYTE *) &semaphore);

   if (result != 0)
      return(-1);


   return(msg_ptr->generic_msg.code);
   }  /* end msg_recv_lcb29k() */




/*
** This function is used to reset the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
exit_comm_lcb29k(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
     return (0);
   }

INT32
reset_comm_lcb29k(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   int  result;
   int  control_reg;

   /*** check for existence of the board ***/

   /* Initialize Control Port Register 0 */
   /* (But don't set LCB29K_RST)         */
   control_reg = (LCB29K_CLRINPC | LCB29K_INTEN | LCB29K_WEN);
   result = outp((unsigned int) PC_port_base,
                 control_reg);

   return(0);
   }  /* end reset_comm_lcb29k() */



/*
** This function is used to "kick" the LCB29K.  This
** amounts to yanking the *RESET line low.  Code
** will begin execution at ROM address 0.
*/

void
go_lcb29k(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
   int  result;
   int  control_reg;

   /* Clear the RST bit in Control Port Register 0 */
   control_reg = (LCB29K_CLRINPC | LCB29K_INTEN | LCB29K_WEN);
   result = outp((unsigned int) PC_port_base,
                 control_reg);

   /* Set the RST bit in Control Port Register 0 */
   control_reg = (LCB29K_RST | LCB29K_INTEN | LCB29K_WEN);
   result = outp((unsigned int) PC_port_base,
                 control_reg);

   }  /* end go_lcb29k() */




/*
** This function is used to write a string of bytes to
** the Am29000 memory on the LCB29K board.
**
*/

INT32
write_memory_lcb29k(memory_space, address, data, byte_count, PC_port_base, PC_mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32	PC_port_base;
   INT32	PC_mem_seg;
   {
   INT32  bytes_in_window;
   INT32  copy_count;
   int    result;

   while (byte_count > 0) {

      /* Write out low order address bits */
      result = outp((unsigned int) (PC_port_base+1),
                    (int) ((address >> 14) & 0xff));

      /* Write out high order address bits */
      if (memory_space == I_MEM)
         result = outp((unsigned int) (PC_port_base+2),
                       (int) (((address >> 22) & 0x7f) | LCB29K_I_MEM));
      else
      if (memory_space == D_MEM)
         result = outp((unsigned int) (PC_port_base+2),
                       (int) (((address >> 22) & 0x7f) | LCB29K_D_MEM));
      else /* Must be either Instruction or Data memory */
         return (-1);

      bytes_in_window = 0x4000 - (address & 0x3fff);
      copy_count = MIN(byte_count, bytes_in_window);

      (void) movedata((unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (unsigned int) PC_mem_seg,
                      (unsigned int) (address & 0x3fff), 
                      (int) copy_count);

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(0);

   }  /* End write_memory_lcb29k() */




/*
** This function is used to read a string of bytes from
** the Am29000 memory on the LCB29K board.   A zero is
** returned if the data is read successfully, otherwise
** a -1 is returned.
*/

INT32
read_memory_lcb29k(memory_space, address, data, byte_count, PC_port_base, PC_mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32	PC_port_base;
   INT32	PC_mem_seg;
   {
   INT32  bytes_in_window;
   INT32  copy_count;
   int    result;

   while (byte_count > 0) {

      /* Write out low order address bits */
      result = outp((unsigned int) (PC_port_base+1),
                    (int) ((address >> 14) & 0xff));

      /* Write out high order address bits */
      if (memory_space == I_MEM)
         result = outp((unsigned int) (PC_port_base+2),
                       (int) (((address >> 22) & 0x7f) | LCB29K_I_MEM));
      else
      if (memory_space == D_MEM)
         result = outp((unsigned int) (PC_port_base+2),
                       (int) (((address >> 22) & 0x7f) | LCB29K_D_MEM));
      else /* Must be either Instruction or Data memory */
         return (-1);

      bytes_in_window = 0x4000 - (address & 0x3fff);
      copy_count = MIN(byte_count, bytes_in_window);

      (void) movedata((unsigned int) PC_mem_seg,
                      (unsigned int) (address & 0x3fff), 
                      (unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (int) copy_count);

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(0);

   }  /* End read_memory_lcb29k() */

INT32
fill_memory_lcb29k()
{
 return (0);
}

