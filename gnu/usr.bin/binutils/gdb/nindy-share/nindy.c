/* This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* This started out life as code shared between the nindy monitor and
   GDB.  For various reasons, this is no longer true.  Eventually, it
   probably should be merged into remote-nindy.c.  */

/******************************************************************************
 *
 *	 		NINDY INTERFACE ROUTINES
 *
 * The caller of these routines should be aware that:
 *
 * (1) ninConnect() should be called to open communications with the
 *     remote NINDY board before any of the other routines are invoked.
 *
 * (2) almost all interactions are driven by the host: nindy sends information
 *     in response to host commands.
 *
 * (3) the lone exception to (2) is the single character DLE (^P, 0x10).
 *     Receipt of a DLE from NINDY indicates that the application program
 *     running under NINDY has stopped execution and that NINDY is now
 *     available to talk to the host (all other communication received after
 *     the application has been started should be presumed to come from the
 *     application and should be passed on by the host to stdout).
 *
 * (4) the reason the application program stopped can be determined with the
 *     ninStopWhy() function.  There are three classes of stop reasons:
 *
 *	(a) the application has terminated execution.
 *	    The host should take appropriate action.
 *
 *	(b) the application had a fault or trace event.
 *	    The host should take appropriate action.
 *
 *	(c) the application wishes to make a service request (srq) of the host;
 *	    e.g., to open/close a file, read/write a file, etc.  The ninSrq()
 *	    function should be called to determine the nature of the request
 *	    and process it.
 */

#include <stdio.h>
#include "defs.h"
#include "serial.h"
#ifdef ANSI_PROTOTYPES
#include <stdarg.h>
#else
#include <varargs.h>
#endif

#if !defined (HAVE_TERMIOS) && !defined (HAVE_TERMIO) && !defined (HAVE_SGTTY)
#define HAVE_SGTTY
#endif

#ifdef HAVE_SGTTY
#include <sys/ioctl.h>
#endif

#include <sys/types.h>	/* Needed by file.h on Sys V */
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>

#if 0
#include "ttycntl.h"
#endif
#include "block_io.h"
#include "wait.h"
#include "env.h"

#define DLE	0x10	/* ^P */
#define XON	0x11	/* ^Q */
#define XOFF	0x13	/* ^S */
#define ESC	0x1b

#define TIMEOUT		-1

int quiet = 0;	/* 1 => stifle unnecessary messages */
serial_t nindy_serial;

static int old_nindy = 0; /* 1 => use old (hex) communication protocol */
static ninStrGet();

		/****************************
		 *                          *
		 *  MISCELLANEOUS UTILTIES  *
		 *                          *
		 ****************************/

/******************************************************************************
 * say:
 *	This is a printf that takes at most two arguments (in addition to the
 *	format string) and that outputs nothing if verbose output has been
 *	suppressed.
 *****************************************************************************/

/* VARARGS */
static void
#ifdef ANSI_PROTOTYPES
say (char *fmt, ...)
#else
say (va_alist)
     va_dcl
#endif
{
  va_list args;
#ifdef ANSI_PROTOTYPES
  va_start(args, fmt);
#else
  char *fmt;

  va_start (args);
  fmt = va_arg (args, char *);
#endif

  if (!quiet)
    {
      vfprintf_unfiltered (gdb_stdout, fmt, args);
      gdb_flush (gdb_stdout);
    }
  va_end (args);
}

/******************************************************************************
 * exists:
 *	Creates a full pathname by concatenating up to three name components
 *	onto a specified base name; optionally looks up the base name as a
 *	runtime environment variable;  and checks to see if the file or
 *	directory specified by the pathname actually exists.
 *
 *	Returns:  the full pathname if it exists, NULL otherwise.
 *		(returned pathname is in malloc'd memory and must be freed
 *		by caller).
 *****************************************************************************/
static char *
exists( base, c1, c2, c3, env )
    char *base;		/* Base directory of path */
    char *c1, *c2, *c3;	/* Components (subdirectories and/or file name) to be
			 *	appended onto the base directory name.  One or
			 *	more may be omitted by passing NULL pointers.
			 */
    int env;		/* If 1, '*base' is the name of an environment variable
			 *	to be examined for the base directory name;
			 *	otherwise, '*base' is the actual name of the
			 *	base directory.
			 */
{
	struct stat buf;/* For call to 'stat' -- never examined */
	char *path;	/* Pointer to full pathname (malloc'd memory) */
	int len;	/* Length of full pathname (incl. terminator) */
	extern char *getenv();


	if ( env ){
		base = getenv( base );
		if ( base == NULL ){
			return NULL;
		}
	}

	len = strlen(base) + 4;
			/* +4 for terminator and "/" before each component */
	if ( c1 != NULL ){
		len += strlen(c1);
	}
	if ( c2 != NULL ){
		len += strlen(c2);
	}
	if ( c3 != NULL ){
		len += strlen(c3);
	}

	path = xmalloc (len);

	strcpy( path, base );
	if ( c1 != NULL ){
		strcat( path, "/" );
		strcat( path, c1 );
		if ( c2 != NULL ){
			strcat( path, "/" );
			strcat( path, c2 );
			if ( c3 != NULL ){
				strcat( path, "/" );
				strcat( path, c3 );
			}
		}
	}

	if ( stat(path,&buf) != 0 ){
		free( path );
		path = NULL;
	}
	return path;
}

		/*****************************
		 *                           *
		 *  LOW-LEVEL COMMUNICATION  *
		 *                           *
		 *****************************/

/* Read *exactly* N characters from the NINDY tty, and put them in
   *BUF.  Translate escape sequences into single characters, counting
   each such sequence as 1 character.

   An escape sequence consists of ESC and a following character.  The
   ESC is discarded and the other character gets bit 0x40 cleared --
   thus ESC P == ^P, ESC S == ^S, ESC [ == ESC, etc.

   Return 1 if successful, 0 if more than TIMEOUT seconds pass without
   any input.  */

static int
rdnin (buf,n,timeout)
    unsigned char * buf;	/* Where to place characters read	*/
    int n;			/* Number of characters to read		*/
    int timeout;		/* Timeout, in seconds			*/
{
  int escape_seen;	/* 1 => last character of a read was an ESC */
  int c;

  escape_seen = 0;
  while (n)
    {
      c = SERIAL_READCHAR (nindy_serial, timeout);
      switch (c)
	{
	case SERIAL_ERROR:
	case SERIAL_TIMEOUT:
	case SERIAL_EOF:
	  return 0;

	case ESC:
	  escape_seen = 1;
	  break;

	default:
	  if (escape_seen)
	    {
	      escape_seen = 0;
	      c &= ~0x40;
	    }
	  *buf++ = c;
	  --n;
	  break;
	}
    }
  return 1;
}


/******************************************************************************
 * getpkt:
 *	Read a packet from a remote NINDY, with error checking, into the
 *	indicated buffer.
 *
 *	Return packet status byte on success, TIMEOUT on failure.
 ******************************************************************************/
static
int
getpkt(buf)
     unsigned char *buf;
{
	int i;
	unsigned char hdr[3];	/* Packet header:
				 *	hdr[0] = low byte of message length
				 *	hdr[1] = high byte of message length
				 *	hdr[2] = message status
				 */
	int cnt;		/* Message length (status byte + data)	*/
	unsigned char cs_calc;	/* Checksum calculated			*/
	unsigned char cs_recv;	/* Checksum received			*/
	static char errfmt[] =
			"Bad checksum (recv=0x%02x; calc=0x%02x); retrying\r\n";

	while (1){
		if ( !rdnin(hdr,3,5) ){
			return TIMEOUT;
		}
		cnt = (hdr[1]<<8) + hdr[0] - 1;
					/* -1 for status byte (already read) */

		/* Caller's buffer may only be big enough for message body,
		 * without status byte and checksum, so make sure to read
		 * checksum into a separate buffer.
		 */
		if ( !rdnin(buf,cnt,5) || !rdnin(&cs_recv,1,5) ){
			return TIMEOUT;
		}

		/* Calculate checksum
		 */
		cs_calc = hdr[0] + hdr[1] + hdr[2];
		for ( i = 0; i < cnt; i++ ){
			cs_calc += buf[i];
		}
		if ( cs_calc == cs_recv ){
			SERIAL_WRITE (nindy_serial, "+", 1);
			return hdr[2];
		}
	
		/* Bad checksum: report, send NAK, and re-receive
		 */
		fprintf(stderr, errfmt, cs_recv, cs_calc );
		SERIAL_WRITE (nindy_serial, "-", 1);
	}
}


/******************************************************************************
 * putpkt:
 *	Send a packet to NINDY, checksumming it and converting special
 *	characters to escape sequences.
 ******************************************************************************/

/* This macro puts the character 'c' into the buffer pointed at by 'p',
 * and increments the pointer.  If 'c' is one of the 4 special characters
 * in the transmission protocol, it is converted into a 2-character
 * escape sequence.
 */
#define PUTBUF(c,p)						\
	if ( c == DLE || c == ESC || c == XON || c == XOFF ){	\
		*p++ = ESC;					\
		*p++ = c | 0x40;				\
	} else {						\
		*p++ = c;					\
	}

static
putpkt( msg, len )
    unsigned char *msg;	/* Command to be sent, without lead ^P (\020) or checksum */
    int len;	/* Number of bytes in message			*/
{
	static char *buf = NULL;/* Local buffer -- build packet here	*/
	static int maxbuf = 0;	/* Current length of buffer		*/
	unsigned char ack;	/* Response received from NINDY		*/
	unsigned char checksum;	/* Packet checksum			*/
	char *p;		/* Pointer into buffer			*/
	int lenhi, lenlo; 	/* High and low bytes of message length	*/
	int i;


	/* Make sure local buffer is big enough.  Must include space for
	 * packet length, message body, and checksum.  And in the worst
	 * case, each character would expand into a 2-character escape
	 * sequence.
	 */
	if ( maxbuf < ((2*len)+10) ){
		if ( buf ){
			free( buf );
		}
		buf = xmalloc( maxbuf=((2*len)+10) );
	}

	/* Attention, NINDY!
	 */
	SERIAL_WRITE (nindy_serial, "\020", 1);


	lenlo = len & 0xff;
	lenhi = (len>>8) & 0xff;
	checksum = lenlo + lenhi;
	p = buf;

	PUTBUF( lenlo, p );
	PUTBUF( lenhi, p );

	for ( i=0; i<len; i++ ){
		PUTBUF( msg[i], p );
		checksum += msg[i];
	}

	PUTBUF( checksum, p );

	/* Send checksummed message over and over until we get a positive ack
	 */
	SERIAL_WRITE (nindy_serial, buf, p - buf);
	while (1){
		if ( !rdnin(&ack,1,5) ){
			/* timed out */
			fprintf(stderr,"ACK timed out; resending\r\n");
			/* Attention, NINDY! */
			SERIAL_WRITE (nindy_serial, "\020", 1);
			SERIAL_WRITE (nindy_serial, buf, p - buf);
		} else if ( ack == '+' ){
			return;
		} else if ( ack == '-' ){
			fprintf( stderr, "Remote NAK; resending\r\n" );
			SERIAL_WRITE (nindy_serial, buf, p - buf);
		} else {
			fprintf( stderr, "Bad ACK, ignored: <%c>\r\n", ack );
		}
	}
}



/******************************************************************************
 * send:
 *	Send a message to a remote NINDY.  Check message status byte
 *	for error responses.  If no error, return NINDY reponse (if any).
 ******************************************************************************/
static
send( out, len, in )
    unsigned char *out;	/* Message to be sent to NINDY			*/
    int len;		/* Number of meaningful bytes in out buffer	*/
    unsigned char *in;	/* Where to put response received from NINDY	*/
{
	char *fmt;
	int status;
	static char *errmsg[] = {
		"",						/* 0 */
		"Buffer overflow",				/* 1 */
		"Unknown command",				/* 2 */
		"Wrong amount of data to load register(s)",	/* 3 */
		"Missing command argument(s)",			/* 4 */
		"Odd number of digits sent to load memory",	/* 5 */
		"Unknown register name",			/* 6 */
		"No such memory segment",			/* 7 */
		"No breakpoint available",			/* 8 */
		"Can't set requested baud rate",		/* 9 */
	};
#	define NUMERRS	( sizeof(errmsg) / sizeof(errmsg[0]) )

	static char err1[] = "Unknown error response from NINDY: #%d\r\n";
	static char err2[] = "Error response #%d from NINDY: %s\r\n";

	while (1){
		putpkt(out,len);
		status = getpkt(in);
		if ( status == TIMEOUT ){
			fprintf( stderr, "Response timed out; resending\r\n" );
		} else {
			break;
		}
	}

	if ( status ){
		fmt =  status > NUMERRS ? err1 : err2;
		fprintf( stderr, fmt, status, errmsg[status] );
		abort();
	}
}

		/************************
		 *                      *
		 *  BAUD RATE ROUTINES  *
		 *                      *
		 ************************/

/* Table of baudrates known to be acceptable to NINDY.  Each baud rate
 * appears both as character string and as a Unix baud rate constant.
 */
struct baudrate {
	char *string;
	int rate;
};

static struct baudrate baudtab[] = {
	 "1200", 1200,
	 "2400", 2400,
	 "4800", 4800,
	 "9600", 9600,
	"19200", 19200,
	"38400", 38400,
	NULL,    0		/* End of table */
};

/******************************************************************************
 * parse_baudrate:
 *	Look up the passed baud rate in the baudrate table.  If found, change
 *	our internal record of the current baud rate, but don't do anything
 *	about the tty just now.
 *
 *	Return pointer to baudrate structure on success, NULL on failure.
 ******************************************************************************/
static
struct baudrate *
parse_baudrate(s)
    char *s;	/* Desired baud rate, as an ASCII (decimal) string */
{
	int i;

	for ( i=0; baudtab[i].string != NULL; i++ ){
		if ( !strcmp(baudtab[i].string,s) ){
			return &baudtab[i];
		}
	}
	return NULL;
}

/******************************************************************************
 * try_baudrate:
 *	Try speaking to NINDY via the specified file descriptor at the
 *	specified baudrate.  Assume success if we can send an empty command
 *	with a bogus checksum and receive a NAK (response of '-') back within
 *	one second.
 *
 *	Return 1 on success, 0 on failure.
 ***************************************************************************/

static int
try_baudrate (serial, brp)
     serial_t serial;
     struct baudrate *brp;
{
  unsigned char c;

  /* Set specified baud rate and flush all pending input */
  SERIAL_SETBAUDRATE (serial, brp->rate);
  tty_flush (serial);

  /* Send empty command with bad checksum, hope for NAK ('-') response */
  SERIAL_WRITE (serial, "\020\0\0\001", 4);

  /* Anything but a quick '-', including error, eof, or timeout, means that
     this baudrate doesn't work.  */
  return SERIAL_READCHAR (serial, 1) == '-';
}

/******************************************************************************
 * autobaud:
 *	Get NINDY talking over the specified file descriptor at the specified
 *	baud rate.  First see if NINDY's already talking at 'baudrate'.  If
 *	not, run through all the legal baudrates in 'baudtab' until one works,
 *	and then tell NINDY to talk at 'baudrate' instead.
 ******************************************************************************/
static
autobaud( serial, brp )
     serial_t serial;
     struct baudrate *brp;
{
  int i;
  int failures;

  say("NINDY at wrong baud rate? Trying to autobaud...\n");
  failures = i = 0;
  while (1)
    {
      say( "\r%s...   ", baudtab[i].string );
      if (try_baudrate(serial, &baudtab[i]))
	{
	  break;
	}
      if (baudtab[++i].string == NULL)
	{
	  /* End of table -- wraparound */
	  i = 0;
	  if ( failures++ )
	    {
	      say("\nAutobaud failed again.  Giving up.\n");
	      exit(1);
	    }
	  else
	    {
	      say("\nAutobaud failed. Trying again...\n");
	    }
	}
    }

  /* Found NINDY's current baud rate; now change it.  */
  say("Changing NINDY baudrate to %s\n", brp->string);
  ninBaud (brp->string);

  /* Change our baud rate back to rate to which we just set NINDY.  */
  SERIAL_SETBAUDRATE (serial, brp->rate);
}

		/**********************************
		 *				  *
		 *   NINDY INTERFACE ROUTINES	  *
		 *                            	  *
		 * ninConnect *MUST* be the first *
		 * one of these routines called.  *
		 **********************************/


/******************************************************************************
 * ninBaud:
 *	Ask NINDY to change the baud rate on its serial port.
 *	Assumes we know the baud rate at which NINDY's currently talking.
 ******************************************************************************/
ninBaud( baudrate )
    char *baudrate;	/* Desired baud rate, as a string of ASCII decimal
			 * digits.
			 */
{
  unsigned char msg[100];

  tty_flush (nindy_serial);

  if (old_nindy)
    {
      char *p;		/* Pointer into buffer	*/
      unsigned char csum;	/* Calculated checksum	*/

      /* Can't use putpkt() because after the baudrate change NINDY's
	 ack/nak will look like gibberish.  */

      for (p=baudrate, csum=020+'z'; *p; p++)
	{
	  csum += *p;
	}
      sprintf (msg, "\020z%s#%02x", baudrate, csum);
      SERIAL_WRITE (nindy_serial, msg, strlen (msg));
    }
  else
    {
      /* Can't use "send" because NINDY reply will be unreadable after
	 baud rate change.  */
      sprintf( msg, "z%s", baudrate );
      putpkt( msg, strlen(msg)+1 );	/* "+1" to send terminator too */
    }
}

/******************************************************************************
 * ninBptDel:
 *	Ask NINDY to delete the specified type of *hardware* breakpoint at
 *	the specified address.  If the 'addr' is -1, all breakpoints of
 *	the specified type are deleted.
 ***************************************************************************/
ninBptDel( addr, type )
    long addr;	/* Address in 960 memory	*/
    char type;	/* 'd' => data bkpt, 'i' => instruction breakpoint */
{
	unsigned char buf[10];

	if ( old_nindy ){
		OninBptDel( addr, type == 'd' ? 1 : 0 );
		return;
	}

	buf[0] = 'b';
	buf[1] = type;

	if ( addr == -1 ){
		send( buf, 2, NULL );
	} else {
		store_unsigned_integer (&buf[2], 4, addr);
		send( buf, 6, NULL );
	}
}


/******************************************************************************
 * ninBptSet:
 *	Ask NINDY to set the specified type of *hardware* breakpoint at
 *	the specified address.
 ******************************************************************************/
ninBptSet( addr, type )
    long addr;	/* Address in 960 memory	*/
    char type;	/* 'd' => data bkpt, 'i' => instruction breakpoint */
{
	unsigned char buf[10];

	if ( old_nindy ){
		OninBptSet( addr, type == 'd' ? 1 : 0 );
		return;
	}


	buf[0] = 'B';
	buf[1] = type;
	store_unsigned_integer (&buf[2], 4, addr);
	send( buf, 6, NULL );
}


/******************************************************************************
 * ninConnect:
 *	Open the specified tty.  Get communications working at the specified
 *	baud rate.  Flush any pending I/O on the tty.
 *
 *	Return the file descriptor, or -1 on failure.
 ******************************************************************************/
int
ninConnect( name, baudrate, brk, silent, old_protocol )
    char *name;		/* "/dev/ttyXX" to be opened			*/
    char *baudrate;/* baud rate: a string of ascii decimal digits (eg,"9600")*/
    int brk;		/* 1 => send break to tty first thing after opening it*/
    int silent;		/* 1 => stifle unnecessary messages when talking to 
			 *	this tty.
			 */
    int old_protocol;
{
	int i;
	char *p;
	struct baudrate *brp;

	/* We will try each of the following paths when trying to open the tty
	 */
	static char *prefix[] = { "", "/dev/", "/dev/tty", NULL };

	if ( old_protocol ){
		old_nindy = 1;
	}

	quiet = silent;		/* Make global to this file */

	for ( i=0; prefix[i] != NULL; i++ ){
		p = xmalloc(strlen(prefix[i]) + strlen(name) + 1 );
		strcpy( p, prefix[i] );
		strcat( p, name );
		nindy_serial = SERIAL_OPEN (p);
		if (nindy_serial != NULL) {
#ifdef TIOCEXCL
			/* Exclusive use mode (hp9000 does not support it) */
			ioctl(nindy_serial->fd,TIOCEXCL,NULL);
#endif
			SERIAL_RAW (nindy_serial);

			if (brk)
			  {
			    SERIAL_SEND_BREAK (nindy_serial);
			  }

			brp = parse_baudrate( baudrate );
			if ( brp == NULL ){
				say("Illegal baudrate %s ignored; using 9600\n",
								baudrate);
				brp = parse_baudrate( "9600" );
			}

			if ( !try_baudrate(nindy_serial, brp) ){
				autobaud(nindy_serial, brp);
			}
			tty_flush (nindy_serial);
			say( "Connected to %s\n", p );
			free(p);
			break;
		}
		free(p);
	}
	return 0;
}

#if 0

/* Currently unused; shouldn't we be doing this on target_kill and
perhaps target_mourn?  FIXME.  */

/******************************************************************************
 * ninGdbExit:
 *	Ask NINDY to leave GDB mode and print a NINDY prompt.
 ****************************************************************************/
ninGdbExit()
{
	if ( old_nindy ){
		OninGdbExit();
		return;
	}
        putpkt((unsigned char *) "E", 1 );
}
#endif

/******************************************************************************
 * ninGo:
 *	Ask NINDY to start or continue execution of an application program
 *	in it's memory at the current ip.
 ******************************************************************************/
ninGo( step_flag )
    int step_flag;	/* 1 => run in single-step mode */
{
	if ( old_nindy ){
		OninGo( step_flag );
		return;
	}
	putpkt((unsigned char *) (step_flag ? "s" : "c"), 1 );
}


/******************************************************************************
 * ninMemGet:
 *	Read a string of bytes from NINDY's address space (960 memory).
 ******************************************************************************/
int
ninMemGet(ninaddr, hostaddr, len)
     long ninaddr;	/* Source address, in the 960 memory space	*/
     unsigned char *hostaddr;	/* Destination address, in our memory space */
     int len;		/* Number of bytes to read			*/
{
	unsigned char buf[BUFSIZE+20];
	int cnt;		/* Number of bytes in next transfer	*/
	int origlen = len;

	if ( old_nindy ){
		OninMemGet(ninaddr, hostaddr, len);
		return;
	}

	for ( ; len > 0; len -= BUFSIZE ){
		cnt = len > BUFSIZE ? BUFSIZE : len;

		buf[0] = 'm';
		store_unsigned_integer (&buf[1], 4, ninaddr);
		buf[5] = cnt & 0xff;
		buf[6] = (cnt>>8) & 0xff;

		send( buf, 7, hostaddr );

		ninaddr += cnt;
		hostaddr += cnt;
	}
	return origlen;
}


/******************************************************************************
 * ninMemPut:
 *	Write a string of bytes into NINDY's address space (960 memory).
 ******************************************************************************/
int
ninMemPut( ninaddr, hostaddr, len )
     long ninaddr;	/* Destination address, in NINDY memory space	*/
     unsigned char *hostaddr;	/* Source address, in our memory space	*/
     int len;		/* Number of bytes to write			*/
{
	unsigned char buf[BUFSIZE+20];
	int cnt;		/* Number of bytes in next transfer	*/
	int origlen = len;

	if ( old_nindy ){
		OninMemPut( ninaddr, hostaddr, len );
		return;
	}
	for ( ; len > 0; len -= BUFSIZE ){
		cnt = len > BUFSIZE ? BUFSIZE : len;

		buf[0] = 'M';
		store_unsigned_integer (&buf[1], 4, ninaddr);
		memcpy(buf + 5, hostaddr, cnt);
		send( buf, cnt+5, NULL );

		ninaddr += cnt;
		hostaddr += cnt;
	}
	return origlen;
}

/******************************************************************************
 * ninRegGet:
 *	Retrieve the contents of a 960 register, and return them as a long
 *	in host byte order.
 *
 *	THIS ROUTINE CAN ONLY BE USED TO READ THE LOCAL, GLOBAL, AND
 *	ip/ac/pc/tc REGISTERS.
 *
 ******************************************************************************/
long
ninRegGet( regname )
    char *regname;	/* Register name recognized by NINDY, subject to the
			 * above limitations.
			 */
{
	unsigned char outbuf[10];
	unsigned char inbuf[20];

	if ( old_nindy ){
		return OninRegGet( regname );
	}

	sprintf( outbuf, "u%s:", regname );
	send( outbuf, strlen(outbuf), inbuf );
	return extract_unsigned_integer (inbuf, 4);
}

/******************************************************************************
 * ninRegPut:
 *	Set the contents of a 960 register.
 *
 *	THIS ROUTINE CAN ONLY BE USED TO SET THE LOCAL, GLOBAL, AND
 *	ip/ac/pc/tc REGISTERS.
 *
 ******************************************************************************/
ninRegPut( regname, val )
    char *regname;	/* Register name recognized by NINDY, subject to the
			 * above limitations.
			 */
    long val;		/* New contents of register, in host byte-order	*/
{
	unsigned char buf[20];
	int len;

	if ( old_nindy ){
		OninRegPut( regname, val );
		return;
	}

	sprintf( buf, "U%s:", regname );
	len = strlen(buf);
	store_unsigned_integer (&buf[len], 4, val);
	send( buf, len+4, NULL );
}

/******************************************************************************
 * ninRegsGet:
 *	Get a dump of the contents of the entire 960 register set.  The
 *	individual registers appear in the dump in the following order:
 *
 *		pfp  sp   rip  r3   r4   r5   r6   r7 
 *		r8   r9   r10  r11  r12  r13  r14  r15 
 *		g0   g1   g2   g3   g4   g5   g6   g7 
 *		g8   g9   g10  g11  g12  g13  g14  fp 
 *		pc   ac   ip   tc   fp0  fp1  fp2  fp3
 *
 *	Each individual register comprises exactly 4 bytes, except for
 *	fp0-fp3, which are 8 bytes.  All register values are in 960
 *	(little-endian) byte order.
 *
 ******************************************************************************/
ninRegsGet( regp )
    unsigned char *regp;		/* Where to place the register dump */
{
	if ( old_nindy ){
		OninRegsGet( regp );
		return;
	}
	send( (unsigned char *) "r", 1, regp );
}


/******************************************************************************
 * ninRegsPut:
 *	Initialize the entire 960 register set to a specified set of values.
 *	The format of the register value data should be the same as that
 *	returned by ninRegsGet.
 *
 * WARNING:
 *	All register values must be in 960 (little-endian) byte order.
 *
 ******************************************************************************/
ninRegsPut( regp )
    char *regp;		/* Pointer to desired values of registers */
{
/* Number of bytes that we send to nindy.  I believe this is defined by
   the protocol (it does not agree with REGISTER_BYTES).  */
#define NINDY_REGISTER_BYTES	((36*4) + (4*8))
	unsigned char buf[NINDY_REGISTER_BYTES+10];

	if ( old_nindy ){
		OninRegsPut( regp );
		return;
	}

	buf[0] = 'R';
	memcpy(buf+1,  regp, NINDY_REGISTER_BYTES );
	send( buf, NINDY_REGISTER_BYTES+1, NULL );
}


/******************************************************************************
 * ninReset:
 *      Ask NINDY to perform a soft reset; wait for the reset to complete.
 *
 ******************************************************************************/
ninReset()
{
	unsigned char ack;

	if ( old_nindy ){
		OninReset();
		return;
	}

	while (1){
		putpkt((unsigned char *) "X", 1 );
		while (1){
			if ( !rdnin(&ack,1,5) ){
				/* Timed out */
				break;		/* Resend */
			}
			if ( ack == '+' ){
				return;
			}
		}
	}
}


/******************************************************************************
 * ninSrq:
 *	Assume NINDY has stopped execution of the 960 application program in
 *	order to process a host service request (srq).  Ask NINDY for the
 *	srq arguments, perform the requested service, and send an "srq
 *	complete" message so NINDY will return control to the application.
 *
 ******************************************************************************/
ninSrq()
{
  /* FIXME: Imposes arbitrary limits on lengths of pathnames and such.  */
	unsigned char buf[BUFSIZE];
	int retcode;
	unsigned char srqnum;
	int i;
	int offset;
	int arg[MAX_SRQ_ARGS];

	if ( old_nindy ){
		OninSrq();
		return;
	}


	/* Get srq number and arguments
	 */
	send((unsigned char *) "!", 1, buf );

	srqnum = buf[0];
	for  ( i=0, offset=1; i < MAX_SRQ_ARGS; i++, offset+=4 ){
		arg[i] = extract_unsigned_integer (&buf[offset], 4);
	}

	/* Process Srq
	 */
	switch( srqnum ){
	case BS_CLOSE:
		/* args: file descriptor */
		if ( arg[0] > 2 ){
			retcode = close( arg[0] );
		} else {
			retcode = 0;
		}
		break;
	case BS_CREAT:
		/* args: filename, mode */
		ninStrGet( arg[0], buf );
		retcode = creat(buf,arg[1]);
		break;
	case BS_OPEN:
		/* args: filename, flags, mode */
		ninStrGet( arg[0], buf );
		retcode = open(buf,arg[1],arg[2]);
		break;
	case BS_READ:
		/* args: file descriptor, buffer, count */
		retcode = read(arg[0],buf,arg[2]);
		if ( retcode > 0 ){
			ninMemPut( arg[1], buf, retcode );
		}
		break;
	case BS_SEEK:
		/* args: file descriptor, offset, whence */
		retcode = lseek(arg[0],arg[1],arg[2]);
		break;
	case BS_WRITE:
		/* args: file descriptor, buffer, count */
		ninMemGet( arg[1], buf, arg[2] );
		retcode = write(arg[0],buf,arg[2]);
		break;
	default:
		retcode = -1;
		break;
	}

	/* Send request termination status to NINDY
	 */
	buf[0] = 'e';
	store_unsigned_integer (&buf[1], 4, retcode);
	send( buf, 5, NULL );
}


/******************************************************************************
 * ninStopWhy:
 *	Assume the application program has stopped (i.e., a DLE was received
 *	from NINDY).  Ask NINDY for status information describing the
 *	reason for the halt.
 *
 *	Returns a non-zero value if the user program has exited, 0 otherwise.
 *	Also returns the following information, through passed pointers:
 *           - why: an exit code if program the exited; otherwise the reason
 *			why the program halted (see stop.h for values).
 *	    - contents of register ip (little-endian byte order)
 *	    - contents of register sp (little-endian byte order)
 *	    - contents of register fp (little-endian byte order)
 ******************************************************************************/
char
ninStopWhy( whyp, ipp, fpp, spp )
    unsigned char *whyp; /* Return the 'why' code through this pointer	*/
    long *ipp;	/* Return contents of register ip through this pointer	*/
    long *fpp;	/* Return contents of register fp through this pointer	*/
    long *spp;	/* Return contents of register sp through this pointer	*/
{
	unsigned char buf[30];
	extern char OninStopWhy ();

	if ( old_nindy ){
		return OninStopWhy( whyp, ipp, fpp, spp );
	}
	send((unsigned char *) "?", 1, buf );

	*whyp = buf[1];
	memcpy ((char *)ipp, &buf[2],  sizeof (*ipp));
	memcpy ((char *)fpp, &buf[6],  sizeof (*ipp));
	memcpy ((char *)spp, &buf[10], sizeof (*ipp));
	return buf[0];
}

/******************************************************************************
 * ninStrGet:
 *	Read a '\0'-terminated string of data out of the 960 memory space.
 *
 ******************************************************************************/
static
ninStrGet( ninaddr, hostaddr )
     unsigned long ninaddr;	/* Address of string in NINDY memory space */
     unsigned char *hostaddr;	/* Address of the buffer to which string should
				 *	be copied.
				 */
{
	unsigned char cmd[5];

	cmd[0] = '"';
	store_unsigned_integer (&cmd[1], 4, ninaddr);
	send( cmd, 5, hostaddr );
}

#if 0
/* Not used.  */

/******************************************************************************
 * ninVersion:
 *	Ask NINDY for version information about itself.
 *	The information is sent as an ascii string in the form "x.xx,<arch>",
 *	where,
 *		x.xx	is the version number
 *		<arch>	is the processor architecture: "KA", "KB", "MC", "CA" *
 *
 ******************************************************************************/
int
ninVersion( p )
     unsigned char *p;		/* Where to place version string */
{

	if ( old_nindy ){
		return OninVersion( p );
	}
	send((unsigned char *) "v", 1, p );
	return strlen(p);
}
#endif /* 0 */
