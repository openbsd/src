static char _[] = "@(#)tdfunc.c	5.25 93/10/28 08:44:32, Srini, AMD.";
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
 * serial port on an Unix-based machine.
 *****************************************************************************
 */

/* This file contains the Target Dependent Functions used by Minimon's
 * Message System.
 */

#include  <stdio.h>

#include  <fcntl.h>
#include  <termio.h>

#ifdef __hpux
#include <sys/modem.h>
#endif

#include  "messages.h"
#include  "tdfunc.h"
#include  "mtip.h"
#include  "macros.h"

/* Serial connection */
/* 
** Serial port routines
*/

/*definitions */
#define BAUD_RATE       B9600
#define CHAR_SIZE         CS8
#define STOP_BITS           0
#define PARITY_ENABLE       0
#define PARITY              0

#define CH0_BUFFER_SIZE  1024

#define BLOCK 1
#define NOBLOCK 0

/* Global for serial */

static	int   msg_port;
static	INT32  in_byte_count=0;

extern	int	BlockCount;

/*
** This function is used to initialize the communication
** channel.  This consists of basically opening the com
** port for reading and writing.
**
** With Sun UNIX, each time the port is opened, the communication
** parameters are reset to default values.  These default values for
** the serial port are currently 9600 baud, 7 bits, even parity.
*/

INT32
init_comm_serial(ignore1, ignore2)
INT32 ignore1;
INT32 ignore2;
   {
   int      result;
   unsigned short  baud;
   struct   termio tbuf;
#ifdef __hpux
   mflag	mbits;
#else
   int		mbits;
#endif
   int		cd;	/* carrier detect */

   /* Open serial port */   
   if ((msg_port = open(tip_config.comm_port, O_NDELAY|O_RDWR)) == -1) {
      return (-1);
   }

   /* Get baud rate */
   if (strcmp(tip_config.baud_rate, "300") == 0)
      baud = B300;
   else
   if (strcmp(tip_config.baud_rate, "600") == 0)
      baud = B600;
   else
   if (strcmp(tip_config.baud_rate, "1200") == 0)
      baud = B1200;
   else
   if (strcmp(tip_config.baud_rate, "2400") == 0)
      baud = B2400;
   else
   if (strcmp(tip_config.baud_rate, "4800") == 0)
      baud = B4800;
   else
   if (strcmp(tip_config.baud_rate, "9600") == 0)
      baud = B9600;
   else
   if (strcmp(tip_config.baud_rate, "19200") == 0)
      baud = B19200;
   else
   if (strcmp(tip_config.baud_rate, "38400") == 0)
      baud = B38400;
   else
      return(-1);


   /* Set up new parameters */
   /* Get termio (for modification) */
   result = ioctl(msg_port, TCGETA, &tbuf);
   if (result == -1)
      return (-1);

   /*
   ** Note:  On a Sun III, the port comes up at 9600 baud,
   ** 7 bits, even parity, with read enabled.  We will change
   ** this to 8 bits, no parity (with RTS/CTS handshaking).
   ** We will also set I/O to "raw" mode.
   */

   /* Set up new parameters */
   tbuf.c_iflag = 0;
   tbuf.c_oflag = 0;
   tbuf.c_cflag = (baud          | CHAR_SIZE | STOP_BITS | CREAD |
                   PARITY_ENABLE | PARITY   );
   tbuf.c_lflag = 0;
   tbuf.c_cc[VMIN] = 0;  /* Number of characters to satisfy read */
#ifdef __hpux
   tbuf.c_cc[VTIME] = 100;  /* intercharacter timer interval in seconds */
#else
   tbuf.c_cc[VTIME] = 1;  /* intercharacter timer interval in seconds */
#endif

   /* Set termio to new mode */
   result = ioctl(msg_port, TCSETA, &tbuf);
   if (result == -1)
      return (-1);

#ifdef __hpux
   /* modem status */
   (void) ioctl (msg_port, MCGETA, &mbits);
   mbits = (MDSR|MDTR|MRTS);
   (void) ioctl (msg_port, MCSETA, &mbits);
#else
   /* modem status */
   (void) ioctl (msg_port, TIOCMGET, &mbits);
   mbits  = (TIOCM_DTR|TIOCM_RTS);
   (void) ioctl (msg_port, TIOCMSET, &mbits);
#endif

   /* FLush queue */
   if (ioctl(msg_port, TCFLSH, 2) == -1)
      return (-1);

   return(0);
   }  /* end init_comm_serial() */


/*
** This function is used to send a message over the
** serial line.
**
** If the message is successfully sent, a zero is
** returned.  If the message was not sendable, a -1
** is returned.  This function blocks.  That is, it
** does not return until the message is completely
** sent, or until an error is encountered.
**
*/

INT32
send_bfr_serial(bfr_ptr, length, port_base, comm_err)
   BYTE  *bfr_ptr;
   INT32 length;
   INT32 port_base;
   INT32 *comm_err;
   {
   int    result;

   /* Send message */
   result = write(msg_port, (char *)bfr_ptr, length);
   if (result != length)
      return (-1);
   else
      return (0);

   }  /* end msg_send_serial() */


/*
** This function is used to receive a message over a
** serial line.
**
** If the message is waiting in the buffer, a zero is
** returned and the buffer pointed to by msg_ptr is filled
** in.  If no message was available, a -1 is returned.
**
*/

/* Read as many characters as are coming and return the number of character
 * read into the buffer.
 * Buffer : pointer to the receiving buffer.
 * nbytes : number of bytes requested.
 * Mode   : Blocking/Non-blocking mode. In blocking mode, this will not
 *          return until atleast a character is received. It is used when
 *          the TIP is to wait for a response from the target, and there is
 *          no need to poll the keyboard.
 * PortBase : not used.
 * CommError : Error during communication.
 */
INT32
recv_bfr_serial(Buffer, nbytes, Mode, PortBase, CommError)
   BYTE  *Buffer;
   INT32 nbytes;
   INT32 Mode;
   INT32 PortBase;
   INT32 *CommError;
   {
   int    result;
   unsigned char	  ch;
   INT32    count;
   int	bcount;
   struct termio	OrigTBuf, NewTBuf;

     count = 0;
     do {
	if (Mode == BLOCK) {
	  bcount = 0;
	  while (bcount++ < BlockCount) {
	    if ((result = read(msg_port, (char *)&ch, 1)) == 1) { /* success */
              *Buffer++ = (BYTE) ch;
	      count = count + 1;
	      bcount = 0;
	    };
	    if (count == nbytes)
	      return (0);
	  };
	  return ((INT32) -1);
	} else { /* non-block */
	  if ((result = read(msg_port, (char *)&ch, 1)) == 1) { /* success */
            *Buffer++ = (BYTE) ch;
	    count = count + 1;
	  } else { /* Timed out */
            return ((INT32) -1);
	  }
	}
     } while (count < nbytes);
     return (0);

#ifdef DEBUG
   if (Mode) { /* BLOCK while reading */
     /*
      * Set blocking mode by set MIN=0 and TIME > 0
      * Here we set TIME to block for 60 seconds.
      */
      (void) ioctl (msg_port, TCGETA, &OrigTBuf);
      (void) ioctl (msg_port, TCGETA, &NewTBuf);
      NewTBuf.c_cc[4] = 0;	/* set MIN to 0 */
      NewTBuf.c_cc[5] = 1;	/* 600 * 0.1 seconds */
      (void) ioctl (msg_port, TCSETA, &NewTBuf);
     count = 0;
     do {
	if (read(msg_port, (char *)&ch, 1) == 1) { /* success */
          *Buffer++ = (BYTE) ch;
	  count = count + 1;
	} else { /* Timed out */
          (void) ioctl (msg_port, TCSETA, &OrigTBuf); /* restore termio */
          return ((INT32) -1);
	}
     } while (count < nbytes);
     (void) ioctl (msg_port, TCSETA, &OrigTBuf); /* restore termio */
     return (0);
   } else { /* Non blocking */
     result = (INT32) -1;
     count = 0;
     while ((count < nbytes) && (read(msg_port, (char *)&ch, 1) == 1)) {
       *Buffer++ = (BYTE) ch;
       count = count + 1;
       result = 0;
     }
     if (count == nbytes) /* read enough */
       return (0);
     else	/* not enough chars read */
       return ((INT32) -1);
   }
#endif
#if 0
   result = read(msg_port, (char *) Buffer, nbytes); /* read as many */
   if (result == nbytes) {
     return (0);
   } else {
     return (-1);
   }
   	if (result > 0) {
      		in_byte_count = in_byte_count + result;
		block_count = 0;
   		if (in_byte_count >= length) {
      			/* Message received */
      			in_byte_count = 0;
      			return(0);
      			}
	} else {

      		/* return if no char & not blocking */
      		if (block == NOBLOCK) return (-1);  

      		/* return if no char, blocking, and past block count */
      		if ((block == BLOCK) && (block_count++ > BlockCount))
         		return (-1);  
      		}
#endif

   }  /* end msg_recv_serial() */


#ifndef	MSDOS
/*
** This function is used to close the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
reset_comm_pcserver(ignore1, ignore2)
INT32	ignore1;
INT32	ignore2;
   {
   unsigned char	  ch;
#ifdef __hpux
    mflag  mbits;
#else
    int  mbits;
#endif

   printf("reset:\n");
   /* Reset message buffer counters */
   in_byte_count = 0;

#ifdef __hpux
   mbits = (MDSR|MDTR|MRTS);
   (void) ioctl (msg_port, MCSETA, &mbits);
#else
   mbits  = (TIOCM_DTR|TIOCM_RTS);
   (void) ioctl (msg_port, TIOCMGET, &mbits);
#endif

   /* Clear data from buffer */
   if (ioctl(msg_port, TCFLSH, 2) == -1) {
     return (-1);
   }

   return(0);
   }  /* end reset_comm_serial() */
#endif

/*
** This function is used to close the communication
** channel.  This is used when resyncing the host and
** target and when exiting the monitor.
*/

INT32
reset_comm_serial(ignore1, ignore2)
INT32	ignore1;
INT32	ignore2;
   {
#ifdef __hpux
   mflag	mbits;
#else
    int  mbits;
#endif

   /* Reset message buffer counters */
   in_byte_count = 0;

#ifdef __hpux
   (void) ioctl (msg_port, MCGETA, &mbits);
   mbits = (MDSR|MDTR|MRTS);
   (void) ioctl (msg_port, MCSETA, &mbits);
#else
   (void) ioctl (msg_port, TIOCMGET, &mbits);
   mbits  = (TIOCM_DTR|TIOCM_RTS);
   (void) ioctl (msg_port, TIOCMSET, &mbits);
#endif

   /* Clear data from buffer */
   if (ioctl(msg_port, TCFLSH, 2) == -1) {
     return (-1);
   }

   return(0);
   }  /* end reset_comm_serial() */


INT32
exit_comm_serial(ignore1, ignore2)
INT32	ignore1;
INT32	ignore2;
   {
   /* Reset message buffer counters */
   in_byte_count = 0;

   (void) close(msg_port);

   return(0);
   }  /* end reset_comm_serial() */
/*
** This function is usually used to "kick-start" the target.
** This is nesessary when targets are shared memory boards.
** With serial communications, this function does nothing.
*/

void
go_serial(port_base, msg_seg)
INT32 port_base;
INT32 msg_seg;
   { return; }


INT32 
write_memory_serial (ignore1, ignore2, ignore3, ignore4, ignore5, ignore6)
	INT32 ignore1;
	ADDR32 ignore2;
	BYTE *ignore3;
	INT32 ignore4; 
	INT32 ignore5;
	INT32 ignore6; 
{ 
	return(-1); }

INT32 
read_memory_serial (ignore1, ignore2, ignore3, ignore4, ignore5, ignore6)
	INT32 ignore1;
	ADDR32 ignore2;
	BYTE *ignore3;
	INT32 ignore4;
	INT32 ignore5;
	INT32 ignore6;
{ return(-1); }

INT32
fill_memory_serial()
   { return(-1); }

/*
** Stubs for PC plug-in board routines
*/

/* EB29K */

INT32  init_comm_eb29k()    {return (FAILURE);}
INT32  msg_send_eb29k()     {return (-1);}
INT32  msg_recv_eb29k()     {return (-1);}
INT32  reset_comm_eb29k()   {return (-1);}
INT32  exit_comm_eb29k()   {return (-1);}
void   go_eb29k()           {}
INT32  read_memory_eb29k()  {return (-1);}
INT32  write_memory_eb29k() {return (-1);}
INT32  fill_memory_eb29k()  {return (-1);}

/* LCB29K */

INT32  init_comm_lcb29k()   {return (FAILURE);}
INT32  msg_send_lcb29k()    {return (-1);}
INT32  msg_recv_lcb29k()    {return (-1);}
INT32  reset_comm_lcb29k()  {return (-1);}
INT32  exit_comm_lcb29k()  {return (-1);}
void   go_lcb29k()          {}
INT32  read_memory_lcb29k() {return (-1);}
INT32  write_memory_lcb29k(){return (-1);}
INT32  fill_memory_lcb29k() {return (-1);}

/* PCEB */

INT32  init_comm_pceb()     {return (FAILURE);}
INT32  msg_send_pceb()      {return (-1);}
INT32  msg_recv_pceb()      {return (-1);}
INT32  reset_comm_pceb()    {return (-1);}
INT32  exit_comm_pceb()    {return (-1);}
void   go_pceb()            {}
INT32  read_memory_pceb()   {return (-1);}
INT32  write_memory_pceb()  {return (-1);}
INT32  fill_memory_pceb()   {return (-1);}

/* EB030 */

INT32  init_comm_eb030()    {return (FAILURE);}
INT32  msg_send_eb030()     {return (-1);}
INT32  msg_recv_eb030()     {return (-1);}
INT32  reset_comm_eb030()   {return (-1);}
INT32  exit_comm_eb030()   {return (-1);}
void   go_eb030()           {}
INT32  read_memory_eb030()  {return (-1);}
INT32  write_memory_eb030() {return (-1);}
INT32  fill_memory_eb030()  {return (-1);}
