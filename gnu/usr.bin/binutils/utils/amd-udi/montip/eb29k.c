static char _[] = "@(#)eb29k.c	5.20 93/10/26 09:57:07, Srini, AMD.";
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
 * This module implements the communications interface between MONTIP and
 * AMD's EB29K PC plug-in card.
 *****************************************************************************
 */

#include <memory.h>
#include <string.h>
#include "eb29k.h"
#include "types.h"
#include "memspcs.h"
#include "macros.h"
#include "mtip.h"
#include "tdfunc.h"

#include <conio.h>
#include <dos.h>

void    endian_cvt PARAMS((union msg_t *, int));
void    tip_convert32 PARAMS ((BYTE *));

/*
** This function is used to initialize the communication
** channel.  This consists of setting the window location
** of the EB29K to the value defined by the values in
** the file eb29k.h.
*/

INT32
init_comm_eb29k(port_base, mem_seg)
INT32 port_base;
INT32 mem_seg;
   {
   int  result;

   /*** check for existence of the board ***/

   /* Set up memory window location */
   result = outp((unsigned int) port_base,
                 ((int) ((mem_seg >> 10) & 0x1f)));
   /* Set base address to zero */
   outp ((unsigned int) (port_base+1), (unsigned int) 0);
   outp ((unsigned int) (port_base+2), (unsigned int) 0);

   return(0);
   }  /* end init_comm_eb29k() */


/*
** This function is used to send a message to the EB29K.
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
msg_send_eb29k(msg_ptr, port_base)
   union  msg_t  *msg_ptr;
   INT32  port_base;
   {
   INT32    result;
   INT32  message_size;

#if 0
   INT32  semaphore;
   /* Set semaphore (EB29K_RECV_BUF_PTR) to zero */
   semaphore = 0;
   result = Mini_write_memory ((INT32)  D_MEM,
                               (ADDR32) EB29K_RECV_BUF_PTR,
                               (INT32)  sizeof(INT32),
                               (BYTE *) &semaphore);
#endif

   /* Get size of whole message */
   message_size = (msg_ptr->generic_msg).length + (2 * sizeof(INT32));

   /* Do endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, OUTGOING_MSG);

   /* Send message */
   result = Mini_write_memory ((INT32)  D_MEM,
                               (ADDR32) EB29K_SEND_BUF,
                               (INT32)  message_size,
                               (BYTE *) msg_ptr);

   if (result != 0)
      return(-1);

   /* Interrupt target (write to EB29K mailbox) */
   result = outp((unsigned int) (port_base+3),
                 (int) 0x00);

   return(0);

   }  /* end msg_send_eb29k() */


/*
** This function is used to receive a message to the EB29K.
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
msg_recv_eb29k(msg_ptr, port_base, Mode)
   union  msg_t  *msg_ptr;
   INT32  port_base;
   INT32  Mode;
   {
   INT32  result;
   ADDR32 recv_buf_addr;
   INT32  parms_length;
   INT32  header_size;
   INT32  semaphore;
   int	retval;


   /* Poll EB29K mailbox */
   /* (If mailbox contains 0xff, a message is waiting) */
   retval = inp((unsigned int) (port_base+3));

   /* If no message waiting, return -1 */
   if (retval != 0xff)
      return (-1);

   /* Get receive buffer address */
   result = Mini_read_memory ((INT32)  D_MEM,
                              (ADDR32) EB29K_RECV_BUF_PTR,
                              (INT32)  sizeof(ADDR32),
                              (BYTE *) &recv_buf_addr);

   if (result != 0) return(-1);

   /* Change endian of recv_buf_addr (if necessary) */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &recv_buf_addr);

   if (recv_buf_addr == 0) return(-1);

   /* Get message header */
   header_size = (INT32) (2 * sizeof(INT32));
   result = Mini_read_memory ((INT32)  D_MEM,
                              (ADDR32) recv_buf_addr,
                              (INT32)  header_size,
                              (BYTE *) msg_ptr);

   if (result != 0) return(-1); 

   /* Get rest of message */
   parms_length = (msg_ptr->generic_msg).length;
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      tip_convert32((BYTE *) &parms_length);
   result = Mini_read_memory ((INT32)  D_MEM,
                              (ADDR32) (recv_buf_addr + header_size),
                              (INT32)  parms_length,
                              (BYTE *) &(msg_ptr->generic_msg.byte));

   if (result != 0) return(-1); 

   /* Do endian conversion */
   if (tip_target_config.TipEndian != tip_target_config.P29KEndian)
      endian_cvt(msg_ptr, INCOMING_MSG);

   /* Write 0xff to EB29K mailbox */
   /* (This tells EB29K that message has been received) */
   retval = outp((unsigned int) (port_base+3), (int) 0xff);

   /* Set semaphore (EB29K_RECV_BUF_PTR) to zero */
   semaphore = 0;
   result = Mini_write_memory ((INT32)  D_MEM,
                               (ADDR32) EB29K_RECV_BUF_PTR,
                               (INT32)  sizeof(INT32),
                               (BYTE *) &semaphore);

   if (result != 0) return(-1);

   return(msg_ptr->generic_msg.code);
   }  /* end msg_recv_eb29k() */

/*
** This function is used to reset the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
exit_comm_eb29k(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {
     return (0);
   }

INT32
reset_comm_eb29k(PC_port_base, PC_mem_seg)
INT32	PC_port_base;
INT32	PC_mem_seg;
   {

   /* Set up memory window location */
   outp((unsigned int) PC_port_base,
                 ((int) ((PC_mem_seg >> 10) & 0x1f)));
   /* Set base address to zero */
   outp ((unsigned int) (PC_port_base+1), (unsigned int) 0);
   outp ((unsigned int) (PC_port_base+2), (unsigned int) 0);
   return(0);
   }  /* end reset_comm_eb29k() */


INT32
fill_memory_eb29k()
   {
   return(0);
   }  



/*
** This function is used to "kick" the EB29K.  This
** amounts to yanking the *RESET line low.  Code
** will begin execution at ROM address 0.
*/

void
go_eb29k(port_base, mem_seg)
INT32 port_base;
INT32 mem_seg;
   {
   int  result;

   /* Toggle the RESET bit in Control Port Register 0 */
   result = outp((unsigned int) port_base,
                 ((int) ((mem_seg >> 10) & 0x1f)));
   result = outp((unsigned int) port_base,
                 ((int) (((mem_seg >> 10) & 0x1f) |
                 EB29K_RESET)));

   }  /* end go_eb29k() */



/*
** This function is used to write a string of bytes to
** the Am29000 memory on the EB29K board.
**
*/

INT32
write_memory_eb29k(memory_space, address, data, byte_count, port_base, mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32    port_base;
   INT32    mem_seg;
   {
   INT32  bytes_in_window;
   INT32  copy_count;
   unsigned char 	MSbit;

  if (address & 0x80000000)
     MSbit = 0x80;
  else
     MSbit = 0x00;

   while (byte_count > 0) {

      /* Write out low order EB29K_addr bits */
      outp((unsigned int) (port_base+1), (int) ((address >> 14) & 0xff));
      /* Write out high order EB29K_addr bits */
     outp((unsigned int) (port_base+2), (int) (((address >> 22) & 0x7f) | MSbit));

      bytes_in_window = 0x4000 - (address & 0x3fff);
      copy_count = MIN(byte_count, bytes_in_window);

      (void) memmove ((void *) ((mem_seg << 16) + (address & 0x3fff)),
		      (void *) data,
		      (size_t) copy_count);
#if 0
      (void) movedata((unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (unsigned int) mem_seg,
                      (unsigned int) (address & 0x3fff), 
                      (int) copy_count);
#endif

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(0);

   }  /* End write_memory_eb29k() */


/*
** This function is used to read a string of bytes from
** the Am29000 memory on the EB29K board.   A zero is
** returned if the data is read successfully, otherwise
** a -1 is returned.
**
*/

INT32
read_memory_eb29k(memory_space, address, data, byte_count, port_base, mem_seg)
   INT32    memory_space;
   ADDR32   address;
   BYTE    *data;
   INT32    byte_count;
   INT32    port_base;
   INT32    mem_seg;
   {
   INT32  bytes_in_window;
   INT32  copy_count;
   unsigned char 	MSbit;

  if (address & 0x80000000)
     MSbit = 0x80;
  else
     MSbit = 0x00;

   while (byte_count > 0) {

      /* Write out low order EB29K_addr bits */
      outp((unsigned int) (port_base+1), (int) ((address >> 14) & 0xff));
      /* Write out high order EB29K_addr bits */
      outp((unsigned int) (port_base+2), (int) (((address >> 22) & 0x7f) | MSbit));

      bytes_in_window = 0x4000 - (address & 0x3fff);
      copy_count = MIN(byte_count, bytes_in_window);

#if 0
      (void) memmove ((void *) data,
		      (void *) ((mem_seg << 16) + (address & 0x3fff)),
		      (size_t) copy_count);
#endif
      (void) movedata((unsigned int) mem_seg,
                      (unsigned int) (address & 0x3fff), 
                      (unsigned int) FP_SEG(data),
                      (unsigned int) FP_OFF(data),
                      (int) copy_count);

      data = data + copy_count;
      address = address + copy_count;
      byte_count = byte_count - copy_count;

      }  /* end while loop */

   return(0);

   }  /* End read_memory_eb29k() */

