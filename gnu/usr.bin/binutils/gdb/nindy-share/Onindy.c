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
 * This version of the NINDY interface routines supports NINDY versions
 * 2.13 and older.  The older versions used a hex communication protocol,
 * instead of the (faster) current binary protocol.   These routines have
 * been renamed by prepending the letter 'O' to their names, to avoid
 * conflict with the current version.  The old versions are kept only for
 * backward compatibility, and well disappear in a future release.
 *
 **************************************************************************/

/* Having these in a separate file from nindy.c is really ugly, and should
   be merged with nindy.c.  */

#include <stdio.h>
#if 0
#include <sys/ioctl.h>
#include <sys/types.h>	/* Needed by file.h on Sys V */
#include <sys/file.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>	/* Needed on Sys V */
#include "ttycntl.h"
#endif
#include "defs.h"
#include "serial.h"

#include "block_io.h"
#include "wait.h"
#include "env.h"

/* Number of bytes that we send to nindy.  I believe this is defined by
   the protocol (it does not agree with REGISTER_BYTES).  */
#define OLD_NINDY_REGISTER_BYTES ((36*4) + (4*8))

extern int quiet;	/* 1 => stifle unnecessary messages */

/* tty connected to 960/NINDY board.  */
extern serial_t nindy_serial;

static OninStrGet();

		/****************************
		 *                          *
		 *  MISCELLANEOUS UTILTIES  *
		 *                          *
		 ****************************/


/******************************************************************************
 * fromhex:
 *	Convert a hex ascii digit h to a binary integer
 ******************************************************************************/
static
int
fromhex( h )
    int h;
{
	if (h >= '0' && h <= '9'){
		h -= '0';
	} else if (h >= 'a' && h <= 'f'){
		h -= 'a' - 10;
	} else {
		h = 0;
	}
	return (h & 0xff);
}


/******************************************************************************
 * hexbin:
 *	Convert a string of ASCII hex digits to a string of binary bytes.
 ******************************************************************************/
static
hexbin( n, hexp, binp )
    int n;		/* Number of bytes to convert (twice this many digits)*/
    char *hexp;		/* Get hex from here		*/
    char *binp;		/* Put binary here		*/
{
	while ( n-- ){
		*binp++ = (fromhex(*hexp) << 4) | fromhex(*(hexp+1));
		hexp += 2;
	}
}


/******************************************************************************
 * binhex:
 *	Convert a string of binary bytes to a string of ASCII hex digits
 ******************************************************************************/
static
binhex( n, binp, hexp )
    int n;              /* Number of bytes to convert   */
    char *binp;         /* Get binary from here         */
    char *hexp;         /* Place hex here               */
{
	static char tohex[] = "0123456789abcdef";

        while ( n-- ){
                *hexp++ = tohex[ (*binp >> 4) & 0xf ];
                *hexp++ = tohex[ *binp & 0xf ];
                binp++;
        }
}

/******************************************************************************
 * byte_order:
 *	If the host byte order is different from 960 byte order (i.e., the
 *	host is big-endian), reverse the bytes in the passed value;  otherwise,
 *	return the passed value unchanged.
 *
 ******************************************************************************/
static
long
byte_order( n )
    long n;
{
	long rev;
	int i;
	static short test = 0x1234;

	if (*((char *) &test) == 0x12) {
		/*
		 * Big-endian host, swap the bytes.
		 */
		rev = 0;
		for ( i = 0; i < sizeof(n); i++ ){
			rev <<= 8;
			rev |= n & 0xff;
			n >>= 8;
		}
		n = rev;
	}
	return n;
}

/******************************************************************************
 * say:
 *	This is a printf that takes at most two arguments (in addition to the
 *	format string) and that outputs nothing if verbose output has been
 *	suppressed.
 ******************************************************************************/
static
say( fmt, arg1, arg2 )
    char *fmt;
    int arg1, arg2;
{
	if ( !quiet ){
		printf( fmt, arg1, arg2 );
		fflush( stdout );
	}
}

		/*****************************
		 *                           *
		 *  LOW-LEVEL COMMUNICATION  *
		 *                           *
		 *****************************/

/* Read a single character from the remote end.  */

static int
readchar()
{
  /* FIXME: Do we really want to be reading without a timeout?  */
  return SERIAL_READCHAR (nindy_serial, -1);
}

/******************************************************************************
 * getpkt:
 *	Read a packet from a remote NINDY, with error checking, and return
 *	it in the indicated buffer.
 ******************************************************************************/
static
getpkt (buf)
     char *buf;
{
	unsigned char recv;	/* Checksum received		*/
	unsigned char csum;	/* Checksum calculated		*/
	char *bp;		/* Poointer into the buffer	*/
	int c;

	while (1){
		csum = 0;
		bp = buf;
		/* FIXME: check for error from readchar ().  */
		while ( (c = readchar()) != '#' ){
			*bp++ = c;
			csum += c;
		}
		*bp = 0;

		/* FIXME: check for error from readchar ().  */
		recv = fromhex(readchar()) << 4;
		recv |= fromhex(readchar());
		if ( csum == recv ){
			break;
		}
	
		fprintf(stderr,
			"Bad checksum (recv=0x%02x; calc=0x%02x); retrying\r\n",
								recv, csum );
		SERIAL_WRITE (nindy_serial, "-", 1);
	}

	SERIAL_WRITE (nindy_serial, "+", 1);
}


/******************************************************************************
 * putpkt:
 *	Checksum and send a gdb command to a remote NINDY, and wait for
 *	positive acknowledgement.
 *
 ******************************************************************************/
static
putpkt( cmd )
    char *cmd;	/* Command to be sent, without lead ^P (\020)
		 * or trailing checksum
		 */
{
	char ack;	/* Response received from NINDY		*/
	char checksum[4];
	char *p;
	unsigned int s;
	char resend;

	for ( s='\020', p=cmd; *p; p++ ){
		s += *p;
	}
	sprintf( checksum, "#%02x",  s & 0xff );

	/* Send checksummed message over and over until we get a positive ack
	 */
	resend = 1;
	do {
		if ( resend ) {
		  SERIAL_WRITE ( nindy_serial, "\020", 1 );
		  SERIAL_WRITE( nindy_serial, cmd, strlen(cmd) );
		  SERIAL_WRITE( nindy_serial, checksum, strlen(checksum) );
		}
		/* FIXME: do we really want to be reading without timeout?  */
		ack = SERIAL_READCHAR (nindy_serial, -1);
		if (ack < 0)
		  {
		    fprintf (stderr, "error reading from serial port\n");
		  }
		if ( ack == '-' ){
			fprintf( stderr, "Remote NAK, resending\r\n" );
			resend = 1;
		} else if ( ack != '+' ){
			fprintf( stderr, "Bad ACK, ignored: <%c>\r\n", ack );
			resend = 0;
		}
	} while ( ack != '+' );
}



/******************************************************************************
 * send:
 *	Send a message to a remote NINDY and return the reply in the same
 *	buffer (clobbers the input message).  Check for error responses
 *	as indicated by the second argument.
 *
 ******************************************************************************/
static
send( buf, ack_required )
    char *buf;		/* Message to be sent to NINDY; replaced by
			 *	NINDY's response.
			 */
    int ack_required;	/* 1 means NINDY's response MUST be either "X00" (no
			 *	error) or an error code "Xnn".
			 * 0 means the it's OK as long as it doesn't
			 *	begin with "Xnn".
			 */
{
	int errnum;
	static char *errmsg[] = {
		"",						/* X00 */
		"Buffer overflow",				/* X01 */
		"Unknown command",				/* X02 */
		"Wrong amount of data to load register(s)",	/* X03 */
		"Missing command argument(s)",			/* X04 */
		"Odd number of digits sent to load memory",	/* X05 */
		"Unknown register name",			/* X06 */
		"No such memory segment",			/* X07 */
		"No breakpoint available",			/* X08 */
		"Can't set requested baud rate",		/* X09 */
	};
#	define NUMERRS	( sizeof(errmsg) / sizeof(errmsg[0]) )

	static char err0[] = "NINDY failed to acknowledge command: <%s>\r\n";
	static char err1[] = "Unknown error response from NINDY: <%s>\r\n";
	static char err2[] = "Error response %s from NINDY: %s\r\n";

	putpkt (buf);
	getpkt (buf);

	if ( buf[0] != 'X' ){
		if ( ack_required ){
			fprintf( stderr, err0, buf );
			abort();
		}

	} else if ( strcmp(buf,"X00") ){
		sscanf( &buf[1], "%x", &errnum );
		if ( errnum > NUMERRS ){
			fprintf( stderr, err1, buf );
		} else{
			fprintf( stderr, err2, buf, errmsg[errnum] );
		}
		abort();
	}
}

		/**********************************
		 *				  *
		 *   NINDY INTERFACE ROUTINES	  *
		 *                            	  *
		 * ninConnect *MUST* be the first *
		 * one of these routines called.  *
		 **********************************/

/******************************************************************************
 * ninBptDel:
 *	Ask NINDY to delete the specified type of *hardware* breakpoint at
 *	the specified address.  If the 'addr' is -1, all breakpoints of
 *	the specified type are deleted.
 ******************************************************************************/
OninBptDel( addr, data )
    long addr;	/* Address in 960 memory	*/
    int data;	/* '1' => data bkpt, '0' => instruction breakpoint */
{
	char buf[100];

	if ( addr == -1 ){
		sprintf( buf, "b%c", data ? '1' : '0' );
	} else {
		sprintf( buf, "b%c%x", data ? '1' : '0', addr );
	}
	return send( buf, 0 );
}


/******************************************************************************
 * ninBptSet:
 *	Ask NINDY to set the specified type of *hardware* breakpoint at
 *	the specified address.
 ******************************************************************************/
OninBptSet( addr, data )
    long addr;	/* Address in 960 memory	*/
    int data;	/* '1' => data bkpt, '0' => instruction breakpoint */
{
	char buf[100];

	sprintf( buf, "B%c%x", data ? '1' : '0', addr );
	return send( buf, 0 );
}

/******************************************************************************
 * ninGdbExit:
 *	Ask NINDY to leave GDB mode and print a NINDY prompt.
 *	Since it'll no longer be in GDB mode, don't wait for a response.
 ******************************************************************************/
OninGdbExit()
{
        putpkt( "E" );
}

/******************************************************************************
 * ninGo:
 *	Ask NINDY to start or continue execution of an application program
 *	in it's memory at the current ip.
 ******************************************************************************/
OninGo( step_flag )
    int step_flag;	/* 1 => run in single-step mode */
{
	putpkt( step_flag ? "s" : "c" );
}


/******************************************************************************
 * ninMemGet:
 *	Read a string of bytes from NINDY's address space (960 memory).
 ******************************************************************************/
OninMemGet(ninaddr, hostaddr, len)
     long ninaddr;	/* Source address, in the 960 memory space	*/
     char *hostaddr;	/* Destination address, in our memory space	*/
     int len;		/* Number of bytes to read			*/
{
  /* How much do we send at a time?  */
#define OLD_NINDY_MEMBYTES 1024
	/* Buffer: hex in, binary out		*/
	char buf[2*OLD_NINDY_MEMBYTES+20];

	int cnt;		/* Number of bytes in next transfer	*/

	for ( ; len > 0; len -= OLD_NINDY_MEMBYTES ){
		cnt = len > OLD_NINDY_MEMBYTES ? OLD_NINDY_MEMBYTES : len;

		sprintf( buf, "m%x,%x", ninaddr, cnt );
		send( buf, 0 );
		hexbin( cnt, buf, hostaddr );

		ninaddr += cnt;
		hostaddr += cnt;
	}
}


/******************************************************************************
 * ninMemPut:
 *	Write a string of bytes into NINDY's address space (960 memory).
 ******************************************************************************/
OninMemPut( destaddr, srcaddr, len )
     long destaddr;	/* Destination address, in NINDY memory space	*/
     char *srcaddr;	/* Source address, in our memory space		*/
     int len;		/* Number of bytes to write			*/
{
	char buf[2*OLD_NINDY_MEMBYTES+20];	/* Buffer: binary in, hex out		*/
	char *p;		/* Pointer into buffer			*/
	int cnt;		/* Number of bytes in next transfer	*/

	for ( ; len > 0; len -= OLD_NINDY_MEMBYTES ){
		cnt = len > OLD_NINDY_MEMBYTES ? OLD_NINDY_MEMBYTES : len;

		sprintf( buf, "M%x,", destaddr );
		p = buf + strlen(buf);
		binhex( cnt, srcaddr, p );
		*(p+(2*cnt)) = '\0';
		send( buf, 1 );

		srcaddr += cnt;
		destaddr += cnt;
	}
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
OninRegGet( regname )
    char *regname;	/* Register name recognized by NINDY, subject to the
			 * above limitations.
			 */
{
	char buf[200];
	long val;

	sprintf( buf, "u%s", regname );
	send( buf, 0 );
	hexbin( 4, buf, (char *)&val );
	return byte_order(val);
}

/******************************************************************************
 * ninRegPut:
 *	Set the contents of a 960 register.
 *
 *	THIS ROUTINE CAN ONLY BE USED TO SET THE LOCAL, GLOBAL, AND
 *	ip/ac/pc/tc REGISTERS.
 *
 ******************************************************************************/
OninRegPut( regname, val )
    char *regname;	/* Register name recognized by NINDY, subject to the
			 * above limitations.
			 */
    long val;		/* New contents of register, in host byte-order	*/
{
	char buf[200];

	sprintf( buf, "U%s,%08x", regname, byte_order(val) );
	send( buf, 1 );
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
 *	fp0-fp3, which are 8 bytes.
 *
 * WARNING:
 *	Each register value is in 960 (little-endian) byte order.
 *
 ******************************************************************************/
OninRegsGet( regp )
    char *regp;		/* Where to place the register dump */
{
	char buf[(2*OLD_NINDY_REGISTER_BYTES)+10];   /* Registers in ASCII hex */

	strcpy( buf, "r" );
	send( buf, 0 );
	hexbin( OLD_NINDY_REGISTER_BYTES, buf, regp );
}

/******************************************************************************
 * ninRegsPut:
 *	Initialize the entire 960 register set to a specified set of values.
 *	The format of the register value data should be the same as that
 *	returned by ninRegsGet.
 *
 * WARNING:
 *	Each register value should be in 960 (little-endian) byte order.
 *
 ******************************************************************************/
OninRegsPut( regp )
    char *regp;		/* Pointer to desired values of registers */
{
	char buf[(2*OLD_NINDY_REGISTER_BYTES)+10];   /* Registers in ASCII hex */

	buf[0] = 'R';
	binhex( OLD_NINDY_REGISTER_BYTES, regp, buf+1 );
	buf[ (2*OLD_NINDY_REGISTER_BYTES)+1 ] = '\0';

	send( buf, 1 );
}


/******************************************************************************
 * ninReset:
 *      Ask NINDY to perform a soft reset; wait for the reset to complete.
 ******************************************************************************/
OninReset()
{

	putpkt( "X" );
	/* FIXME: check for error from readchar ().  */
	while ( readchar() != '+' ){
		;
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
OninSrq()
{
  /* FIXME: Imposes arbitrary limits on lengths of pathnames and such.  */
	char buf[BUFSIZE];
	int retcode;
	unsigned char srqnum;
	char *p;
	char *argp;
	int nargs;
	int arg[MAX_SRQ_ARGS];


	/* Get srq number and arguments
	 */
	strcpy( buf, "!" );
	send( buf, 0 );
	hexbin( 1, buf, (char *)&srqnum );

	/* Set up array of pointers the each of the individual
	 * comma-separated args
	 */
	nargs=0;
	argp = p = buf+2;
        while ( 1 ){
                while ( *p != ',' && *p != '\0' ){
                        p++;
                }
                sscanf( argp, "%x", &arg[nargs++] );
                if ( *p == '\0' || nargs == MAX_SRQ_ARGS ){
                        break;
                }
                argp = ++p;
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
		OninStrGet( arg[0], buf );
		retcode = creat(buf,arg[1]);
		break;
	case BS_OPEN:
		/* args: filename, flags, mode */
		OninStrGet( arg[0], buf );
		retcode = open(buf,arg[1],arg[2]);
		break;
	case BS_READ:
		/* args: file descriptor, buffer, count */
		retcode = read(arg[0],buf,arg[2]);
		if ( retcode > 0 ){
			OninMemPut( arg[1], buf, retcode );
		}
		break;
	case BS_SEEK:
		/* args: file descriptor, offset, whence */
		retcode = lseek(arg[0],arg[1],arg[2]);
		break;
	case BS_WRITE:
		/* args: file descriptor, buffer, count */
		OninMemGet( arg[1], buf, arg[2] );
		retcode = write(arg[0],buf,arg[2]);
		break;
	default:
		retcode = -1;
		break;
	}

	/* Tell NINDY to continue
	 */
	sprintf( buf, "e%x", retcode );
	send( buf, 1 );
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
OninStopWhy( whyp, ipp, fpp, spp )
    char *whyp;	/* Return the 'why' code through this pointer	*/
    char *ipp;	/* Return contents of register ip through this pointer	*/
    char *fpp;	/* Return contents of register fp through this pointer	*/
    char *spp;	/* Return contents of register sp through this pointer	*/
{
	char buf[30];
	char stop_exit;

	strcpy( buf, "?" );
	send( buf, 0 );
	hexbin( 1, buf, &stop_exit );
	hexbin( 1, buf+2, whyp );
	hexbin( 4, buf+4, ipp );
	hexbin( 4, buf+12, fpp );
	hexbin( 4, buf+20, spp );
	return stop_exit;
}

/******************************************************************************
 * ninStrGet:
 *	Read a '\0'-terminated string of data out of the 960 memory space.
 *
 ******************************************************************************/
static
OninStrGet( ninaddr, hostaddr )
     unsigned long ninaddr;	/* Address of string in NINDY memory space */
     char *hostaddr;		/* Address of the buffer to which string should
				 *	be copied.
				 */
{
  /* FIXME: seems to be an arbitrary limit on the length of the string.  */
	char buf[BUFSIZE];	/* String as 2 ASCII hex digits per byte */
	int numchars;		/* Length of string in bytes.		*/

	sprintf( buf, "\"%x", ninaddr );
	send( buf, 0 );
	numchars = strlen(buf)/2;
	hexbin( numchars, buf, hostaddr );
	hostaddr[numchars] = '\0';
}

#if 0
/* never used.  */

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
OninVersion( p )
     char *p;		/* Where to place version string */
{
  /* FIXME: this is an arbitrary limit on the length of version string.  */
	char buf[BUFSIZE];

	strcpy( buf, "v" );
	send( buf, 0 );
	strcpy( p, buf );
	return strlen( buf );
}
#endif
