static char _[] = "@(#)io.c	5.22 93/10/26 14:50:43, Srini, AMD.";
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
 * This file contains the I/O related routines.
 *****************************************************************************
 */

#include  <stdio.h>
#include  <string.h>
#ifdef	MSDOS
#include  <conio.h>
#else
#include   <sys/ioctl.h>
#endif
#include  "main.h"
#include  "miniint.h"
#include  "error.h"
#include  "monio.h"

/* Function declarations */

INT32	Mini_io_setup PARAMS((void));
INT32	Mini_io_reset PARAMS((void));
int	getkey PARAMS((void));
INT32	Mini_poll_kbd PARAMS((char *cmd_buffer, int size, int mode));
int	cmd_io PARAMS ((char *cmd_buffer, char c));
int	channel0_io PARAMS ((char c));

INT32
Mini_io_setup()
{
   setbuf(stdout, 0);	/* stdout unbuffered */
   return(SUCCESS);
}

INT32
Mini_io_reset()
{
/* Nothing special for now */
 return(SUCCESS);
}

/*
** This function is used to perform all host I/O.  It
** calls the functions cmd_io() or hif_io() as appropriate
** Note that there are eight pobible I/O "modes".  These
** are all possible combination of:
**
**          - Host / Target I/O
**          - HIF / non-HIF I/O
**          - Command file / keyboard I/O
**
*/

INT32
Mini_poll_kbd(cmd_buffer, size, blockmode)
char	*cmd_buffer;
int	size;
int	blockmode;
{
#ifdef	MSDOS
   char		ch;
   static int	indx=0;

   io_config.cmd_ready = FALSE;
   if (blockmode) { /* BLOCK until a command is typed (line buffered) */
     while (gets(cmd_buffer) == NULL); /* no characters in stdin */
     io_config.cmd_ready = TRUE;
   } else { /* NONBLOCk return immediately if there is no command pending */
     if (kbhit()) {
       ch = (unsigned char) getche();
       *(cmd_buffer+indx) = ch;
       indx=indx+1;
       if (ch == (unsigned char) 13) { /* \r, insert \n */
	     putchar(10);	/* line feed */
	     *(cmd_buffer+indx) = '\0';
             io_config.cmd_ready = TRUE;
	     indx=0;
       } else if (ch == (unsigned char) 8) { /* backspace */
	 indx=indx-1;
       } else if (ch == (unsigned char) 127) { /* delete */
	 indx=indx-1;
       }
     };
   }
   return(SUCCESS);

#else
   int   c;
   int   result;
   char *temp_ptr;
   int		tries;
   int		i;

   result = 0;
   io_config.cmd_ready = FALSE;

   if (blockmode)  {	/* block mode read */
      i = 0;
#ifdef __hpux
      ioctl(fileno(stdin), FIOSNBIO, &i);	/* set blocking read */
#else
      ioctl(fileno(stdin), FIONBIO, &i);	/* set blocking read */
#endif
   } else	{	/* nonblocking read */
   		/* for now only read from stdin */
      i = 1;
#ifdef __hpux
      ioctl(fileno(stdin), FIOSNBIO, &i);	/* set non blocking read */
#else
      ioctl(fileno(stdin), FIONBIO, &i);	/* set non blocking read */
#endif
   }

   /* Now read from stdin. */
   result = read( 0, cmd_buffer, BUFSIZ );

   if (result < 0)
   {
   } else {
      cmd_buffer[result] = '\0';
      io_config.cmd_ready = TRUE;
   }

   if (blockmode) {
   } else {
      i = 0;
#ifdef __hpux
      ioctl(fileno(stdin), FIOSNBIO, &i);   /* clear non-blocking read */
#else
      ioctl(fileno(stdin), FIONBIO, &i);   /* clear non-blocking read */
#endif
   }

   return(SUCCESS);
#endif
}

