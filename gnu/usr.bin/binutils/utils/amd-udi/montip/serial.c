static char _[] = "@(#)serial.c	5.21 93/10/26 09:47:06, Srini, AMD.";
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
 * This module contains the functions to initialize, read, and write to the
 * serial ports (COM1, COM2,...) on a PC.
 *****************************************************************************
 */

#include <stdio.h>
#include <conio.h>
#include <bios.h>
#include <dos.h>  
#include <string.h>
#include "types.h"
#include "memspcs.h"
#include "messages.h"
#include "mtip.h"
#include "tdfunc.h"

/* Serial Port Defs */
 /*
 * Divisors for different baud rates to be used to initialize DLA
 * register.
 */
#define	_DIV_COM_110	1047
#define	_DIV_COM_150	768
#define	_DIV_COM_300	384
#define	_DIV_COM_600	192
#define	_DIV_COM_1200	96
#define	_DIV_COM_2400	48
#define	_DIV_COM_4800	24
#define	_DIV_COM_9600	12
#define	_DIV_COM_19200	6
#define	_DIV_COM_38400	3
#define	_DIV_COM_115200	1

#define	LCR_DLAB	0x80

#define	DLA_LOW_OFFSET	0x0


/*
** Definitions
*/

#define BUF_SIZE               2048

/*
** This data structure is used by the interrupt driven
** serial I/O.
*/

struct serial_io_t {
   int error;                      /* Error code */
   unsigned int    port;           /* Port number */
   unsigned int    port_code;      /* Port code (for bios calls) */
   unsigned int    int_number;     /* Port interrupt number      */
   unsigned int    int_mask;       /* Port interrupt mask        */
   unsigned int    baud;           /* Port baud rate             */
   unsigned int    old_vector_ds;  /* Interrupt vector (old)     */
   unsigned int    old_vector_dx;
   volatile
   unsigned char  *start;          /* Start of ring buffer       */
   volatile
   unsigned char  *end;            /* End of ring buffer         */
   };

static unsigned char   serial_io_buffer[BUF_SIZE];

/* These definitions are from bios.h */
#define CHAR_SIZE         _COM_CHR8
#define STOP_BITS        _COM_STOP1
#define PARITY        _COM_NOPARITY

/*
** Serial port definitions
*/

#define INTR_MASK    0x21    /* 8259 Interrupt Mask Port */
#define INTR_EOI     0x20    /* 8259 EOI Port */

#define COM1         0x3f8   /* COM1 Port Base */
#define COM1_CODE    0x00    /* COM1 Port Code */
#define COM1_INT     0x0c    /* COM1 Interrupt Number */
#define COM1_MASK    0x10    /* COM1 Interrupt Mask (IRQ4) */

#define COM2         0x2f8   /* COM2 Port Base */
#define COM2_CODE    0x01    /* COM2 Port Code */
#define COM2_INT     0x0b    /* COM2 Interrupt Number */
#define COM2_MASK    0x08    /* COM2 Interrupt Mask (IRQ3) */

#define MSR_OFFSET   0x6     /* Modem Status Register offset */
#define LSR_OFFSET   0x5     /* Line status Register offset */
#define MCR_OFFSET   0x4     /* Modem Control Register offset */
#define LCR_OFFSET   0x3     /* Line Control Register offest */
#define	IID_OFFSET   0x2     /* Interrupt pending register */
#define IER_OFFSET   0x1     /* Interrupt Enable Register offest */

/* Bits in Line Status Register (LSR) */
#define AC1   0x80    /* Always clear */
#define TSRE  0x40    /* Transmitter Shift Register Empty */
#define THRE  0x20    /* Transmitter Holding Register Empty */
#define BI    0x10    /* Break Interrupt */
#define FE    0x08    /* Framing Error */
#define PE    0x04    /* Parity Error */
#define OE    0x02    /* Overrun Error */
#define DR    0x01    /* Data Ready */

/* Bits in Modem Control Register */
#define CD    0x80
#define RI    0x40
#define DSR   0x20
#define CTS   0x10
#define OUT2  0x08
#define RTS   0x02
#define DTR   0x01

#define MAX_BLOCK  	1000	

/*  function prototypes */

void   endian_cvt PARAMS((union msg_t *, int));
void   tip_convert32 PARAMS((BYTE *));
INT32	init_parport (char *);

void   interrupt far serial_int PARAMS((void));
void	(interrupt far *OldVector)();
int    get_byte_serial PARAMS((void));

extern	int	BlockCount;
extern	int	lpt_initialize;
/* globals */

struct serial_io_t serial_io;

INT32  in_msg_length=0;
INT32  in_byte_count=0;

/*
** Serial Port functions
*/

/*
** This function is used to initialize the communication
** channel.  First the serial_io data structure is
** initialized.  Then the new interrupt vector is installed.
** Finally, the port is initialized, with DTR, RTS and OUT2
** set.
**
*/

INT32 write_memory_serial (ignore1, ignore2, ignore3, ignore4, ignore5, ignore6)
	INT32 ignore1;
	ADDR32 ignore2;
	BYTE *ignore3;
	INT32 ignore4; 
	INT32 ignore5;
	INT32 ignore6; 
{ 
	return(-1); }

INT32 read_memory_serial (ignore1, ignore2, ignore3, ignore4, ignore5, ignore6)
	INT32 ignore1;
	ADDR32 ignore2;
	BYTE *ignore3;
	INT32 ignore4;
	INT32 ignore5;
	INT32 ignore6;
{ return(-1); }

INT32 fill_memory_serial() { return(-1); }

INT32
init_comm_serial(ignore1, ignore2)
INT32 ignore1;
INT32 ignore2;
   {
   unsigned result;
   unsigned config;
   unsigned   int comm_status;

   /* Initialize serial_io */
   serial_io.error = FALSE;

   /* Set up port number */
   if ((strcmp(tip_config.comm_port, "com1") == 0) ||
       (strcmp(tip_config.comm_port, "com1:") == 0)) {
      serial_io.port = COM1;
      serial_io.port_code = COM1_CODE;
      serial_io.int_number = COM1_INT;
      serial_io.int_mask = COM1_MASK;
      }
   else
   if ((strcmp(tip_config.comm_port, "com2") == 0) ||
       (strcmp(tip_config.comm_port, "com2:") == 0)) {
      serial_io.port = COM2;
      serial_io.port_code = COM2_CODE;
      serial_io.int_number = COM2_INT;
      serial_io.int_mask = COM2_MASK;
      }
   else
      return((INT32) -1);

    /* Check status */
    comm_status = inp(serial_io.port+LSR_OFFSET);
#if 0
    /* reset any communication errors */
    outp(serial_io.port+LSR_OFFSET, 
		       (unsigned int) (comm_status & ~(FE|PE|OE)));
#endif
			       

   /* Get baud rate (Note: MS-DOS only goes to 9600) */
   outp (serial_io.port+LCR_OFFSET, LCR_DLAB);

   if (strcmp(tip_config.baud_rate, "110") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_110);
   else
   if (strcmp(tip_config.baud_rate, "150") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_150);
   else
   if (strcmp(tip_config.baud_rate, "300") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_300);
   else
   if (strcmp(tip_config.baud_rate, "600") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_600);
   else
   if (strcmp(tip_config.baud_rate, "1200") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_1200);
   else
   if (strcmp(tip_config.baud_rate, "2400") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_2400);
   else
   if (strcmp(tip_config.baud_rate, "4800") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_4800);
   else
   if (strcmp(tip_config.baud_rate, "9600") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_9600);
   else
   if (strcmp(tip_config.baud_rate, "19200") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_19200);
   else
   if (strcmp(tip_config.baud_rate, "38400") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_38400);
   else
   if (strcmp(tip_config.baud_rate, "115200") == 0)
      outpw (serial_io.port+DLA_LOW_OFFSET, _DIV_COM_115200);
   else
      return((INT32) -1);  /* EMBAUD); */

   /* Set LCR */
   outp (serial_io.port+LCR_OFFSET, 
		(unsigned int) (_COM_CHR8|_COM_STOP1|_COM_NOPARITY));

   /* Save old interrupt vector */
   OldVector = _dos_getvect (serial_io.int_number);

   /* Initialize ring buffer */
   serial_io.start = serial_io_buffer;
   serial_io.end = serial_io_buffer;

   /* Install interrupt vector */
   /* Note:  the interrupt handler should be in the same code */
   /*        segment as this function.  We will use CS for    */
   /*        the segment offset value.                        */

   _dos_setvect(serial_io.int_number, serial_int); /* new handler */

   /* Turn on DTR, RTS and OUT2 */
   result = outp((serial_io.port+MCR_OFFSET), (DTR | RTS | OUT2));

   /* Enable interrupt on serial port controller */
   result = outp((serial_io.port+IER_OFFSET), 0x01);

   /* Set interrupt mask on 8259 */
   config = inp(INTR_MASK);  /* Get current 8259 mask */
   result = outp(INTR_MASK, (config & ~serial_io.int_mask));

   /* Set global message indices */
   in_msg_length = 0;
   in_byte_count = 0;

   /* initialize parallel port */
   if (lpt_initialize)
      return (init_parport(tip_config.par_port));

   return((INT32) 0);
   }  /* end init_comm_serial() */

/*
** This function is used to send bytes over the serial line.
** If the bytes are successfully sent, a zero is returned.  
** If the bytes are not sent, a -1 is returned.
*/

INT32
send_bfr_serial(bfr_ptr, length, port_base, comm_err)
   BYTE   *bfr_ptr;
   INT32  length;
   INT32  port_base;
   INT32  *comm_err;
   {
   int        retries;
   INT32      byte_count = 0;
   unsigned   int comm_status;
   unsigned   int result;

   /* Send message */
   retries = 0;
   do {

      /* check user interrupt */
      SIGINT_POLL
      /* Check if data ready */
      comm_status = inp(serial_io.port+LSR_OFFSET);

      /* Check for communication errors */
      if ((comm_status & (FE | PE | OE)) != 0) {
	  *comm_err = 1;
	  return (-1);
      }

      /* If Transmitter Holding Register Empty (THRE) */
      /* send out data */
      if ((comm_status & THRE) != 0) {
         result = outp(serial_io.port, bfr_ptr[byte_count]);
         byte_count = byte_count + 1;
         retries = 0;
         } else {
            retries = retries + 1;
            if (retries >= 20000)   
               return (-1);   /* EMNOSEND); */
            }

      } while (byte_count < length );  

   return(0);
   }  /* end send_bfr_serial() */

/*
** This function is used to receive bytes over a serial line.
**
** If block equals NONBLOCK then the function returns as soon
** there are no bytes remaining in the UART.           
** If block equals BLOCK then the function waits until all
** bytes are gotten before returning.
** 
** If all bytes requested are gotten, 0 is returned, else -1.
*/

INT32
recv_bfr_serial(bfr_ptr, length, block, port_base, comm_err)
   BYTE   *bfr_ptr;
   INT32  length;
   INT32  block;
   INT32  port_base;
   INT32  *comm_err;
   {
   int        comm_status;
   int        c;
   int        result;  
   int        bytes_free;

   int      block_count = 0;

   /* Loop as long as characters keep coming */
   for (;;) {

      /* Check for communication errors */
      comm_status = inp(serial_io.port+LSR_OFFSET);
      if ((comm_status & (FE | PE | OE)) != 0)
	  {
	  *comm_err = 1;
	  return (-1);
	  }

      /* Check for buffer overflow */
      if (serial_io.error == TRUE)
 	  {
	  *comm_err = 1;
	  return (-1);
 	  }

      /* Do flow control.  If the buffer is 9/10 full, */
      /* deassert DTR and RTS.  If the buffer becomes */
      /* 1/10 full, reassert DTR and RTS.              */
      bytes_free = (int) (serial_io.start - serial_io.end);
      if (bytes_free <= 0)
         bytes_free = BUF_SIZE + bytes_free;

      comm_status = inp(serial_io.port+MCR_OFFSET);
      if (bytes_free <= (BUF_SIZE/10))
         result = outp((serial_io.port+MCR_OFFSET),
                       (comm_status & ~DTR & ~RTS));

      if (bytes_free >= ((9*BUF_SIZE)/10))
         result = outp((serial_io.port+MCR_OFFSET),
                       (comm_status | DTR | RTS));

      /* Get character */
      c = get_byte_serial();

      /* return if no char & not blocking */
      if ((c == -1) && (block == NONBLOCK))
         return (-1);  

      /* return if no char, blocking, and past block count */
      if ((c == -1) && (block == BLOCK) && (block_count++ > BlockCount))
         return (-1);  

      /* Save byte in bfr_ptr buffer */
      if (c != -1) {
      	  bfr_ptr[in_byte_count] = (BYTE) c;
	  block_count = 0;
      	  in_byte_count = in_byte_count + 1;
	  }

      /* Message received ? */
      if (in_byte_count == length) {
         in_byte_count = 0;
         return(0);
         }
      }  	/* end for(;;) */
   }  		/* end recv_bfr_serial() */


/*
** This function is used to reset the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
reset_comm_serial(ignore1, ignore2)
INT32	ignore1;
INT32	ignore2;
   {
   unsigned   int status;

#define	CLEAR_STAT	(int) 1

  do {
    /* Clear LSR */
    inp(serial_io.port+LSR_OFFSET);
    /* Clear RX reg */
    inp (serial_io.port);
    /* Clear MSR */
    inp (serial_io.port+MSR_OFFSET);
    /* interrupt pending ? */
    status = inp(serial_io.port+IID_OFFSET);
  } while (status != CLEAR_STAT);

#if 0
    /* reset any communication errors */
    outp(serial_io.port+LSR_OFFSET, 
		       (unsigned int) (comm_status & ~(FE|PE|OE)));
#endif
			       
   /* Initialize serial_io */
   serial_io.error = FALSE;

   /* Initialize ring buffer */
   serial_io.start = serial_io_buffer;
   serial_io.end = serial_io_buffer;

   /* Set global message indices */
   in_msg_length = 0;
   in_byte_count = 0;

   return((INT32) 0);
   }  /* end reset_comm_serial() */


INT32
exit_comm_serial(ignore1, ignore2)
INT32	ignore1;
INT32	ignore2;
   {
   /* Initialize serial_io */
   serial_io.error = FALSE;

   /* Initialize ring buffer */
   serial_io.start = serial_io_buffer;
   serial_io.end = serial_io_buffer;

   /* Set global message indices */
   in_msg_length = 0;
   in_byte_count = 0;

   /* install old handler back */
   _dos_setvect(serial_io.int_number, OldVector);

   return((INT32) 0);
   }  /* end reset_comm_serial() */

/*
** This function is usually used to "kick-start" the target.
** This is nesessary when targets are shared memory boards.
** With serial communications, this function does nothing.
*/

void
go_serial(ignore1, ignore2)
INT32 ignore1;
INT32 ignore2;
   {
   return;
   }  /* end go_serial() */



/*
** This function is used to get a byte from the the
** serial_io_buffer.  The data in this buffer is written
** by the interrupt handler.
**
** If no data is available, a -1 is returned.  Otherwise
** a character is returned.
*/

int
get_byte_serial()
   {
   int result=-1;

      /* Turn interrupts off while reading buffer */
     _disable();

   /* No bytes available */
   if (serial_io.start == serial_io.end)
      result = -1;
   else {

      /* Return character */
      result = (int) *serial_io.start;
      serial_io.start++;
      /* Check for wrap around */
      if (serial_io.start >= (serial_io_buffer+BUF_SIZE)) {
         serial_io.start = serial_io_buffer;         
      }

    }
      /* Turn interrupts back on */
      _enable();

   return (result);
   }  /* end get_byte_serial() */



/*
** This function is the interrupt handler which buffers
** incoming characters.
**
** Note:  The "interrupt" keyword is not well documented.
**        It produces a procedure which returns with an
**        "iret" instead of the usual "ret".
*/

void interrupt serial_int()
   {
   int c;

   /* Get character */
   c = inp(serial_io.port);

   *serial_io.end = (unsigned char) c;
   serial_io.end++;
   /* Check for wrap around */
   if (serial_io.end >= (serial_io_buffer+BUF_SIZE))
      serial_io.end = serial_io_buffer;         

   /* Has the buffer overflowed? */
   if (serial_io.start == serial_io.end)
      serial_io.error = TRUE;

   /* Send EOI to 8259 */
   (void) outp(INTR_EOI, 0x20);

   }  /* end serial_int() */


