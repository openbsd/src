/* -*-C-*- */

#define PERL_NO_GET_CONTEXT     /* we want efficiency */
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "ppport.h"

#define InputStream PerlIO *

/*******************************************************************

 Copyright (C) 1994,1995,1996,1997 Kenneth Albanowski. Unlimited
 distribution and/or modification is allowed as long as this copyright
 notice remains intact.

 Written by Kenneth Albanowski on Thu Oct  6 11:42:20 EDT 1994
 Contact at kjahds@kjahds.com or CIS:70705,126

 Maintained by Jonathan Stowe <jns@gellyfish.co.uk>

 The below captures the history prior to it being in full time version
 control:

 $Id: ReadKey.xs,v 2.22 2005/01/11 21:15:17 jonathan Exp $

 Version 2.21, Sun Jul 28 12:57:56 BST 2002
    Fix to improve the chances of automated testing succeeding

 Version 2.20, Tue May 21 07:52:47 BST 2002
    Patch from Autrijus Tang fixing Win32 Breakage with bleadperl
    
 Version 2.19, Thu Mar 21 07:25:31 GMT 2002
    Added check for definedness of $_[0] in comparisons in ReadKey, ReadLine
    after reports of warnings.

 Version 2.18, Sun Feb 10 13:06:57 GMT 2002
    Altered prototyping style after reports of compile failures on
    Windows.

 Version 2.17, Fri Jan 25 06:58:47 GMT 2002
    The '_' macro for non-ANSI compatibility was removed in 5.7.2

 Version 2.16, Thu Nov 29 21:19:03 GMT 2001
    It appears that the genchars.pl bit of the patch didnt apply
    Applied the new ppport.h from Devel::PPPort

 Version 2.15, Sun Nov  4 15:02:37 GMT 2001 (jns)
    Applied the patch in 
    http://www.xray.mpe.mpg.de/mailing-lists/perl5-porters/2001-01/msg01588.html
    for PerlIO compatibility.

 Version 2.14, Sun Mar 28 23:26:13 EST 1999
    ppport.h 1.007 fixed for 5.005_55.
 
 Version 2.13, Wed Mar 24 20:46:06 EST 1999
 	Adapted to ppport.h 1.006.

 Version 2.12, Wed Jan  7 10:33:11 EST 1998
 	Slightly modified test and error reporting for Win32.
 
 Version 2.11, Sun Dec 14 00:39:12 EST 1997
    First attempt at Win32 support.

 Version 2.10, skipped

 Version 2.09, Tue Oct  7 13:07:43 EDT 1997
    Grr. Added explicit detection of sys/poll.h and poll.h.

 Version 2.08, Mon Oct  6 16:07:44 EDT 1997
    Changed poll.h to sys/poll.h.

 Version 2.07, Sun Jan 26 19:11:56 EST 1997
    Added $VERSION to .pm.

 Version 2.06, Tue Nov 26 01:47:09 EST 1996
    Added PERLIO support and removed duplicate declaration in .pm.

 Version 2.05, Tue Mar 12 19:08:33 EST 1996
 	Changed poll support so it works. Cleaned up .pm a little.
 	
 Version 2.04, Tue Oct 10 05:35:48 EDT 1995
 	Whoops. Changed GetTermSize back so that GSIZE code won't be
 	compiled if GWINSZ is being used. Also took ts_xxx and ts_yyy
 	out of GSIZE.

 Version 2.03, Thu Sep 21 21:53:16 EDT 1995
	Fixed up debugging info in Readkey.pm, and changed TermSizeVIO
	to use _scrsize(). Hopefully this is GO for both Solaris and OS/2.

 Version 2.02, Mon Sep 18 22:17:57 EDT 1995
	Workaround for Solaris bug wasn't sufficient. Modularlized
	GetTermSize into perl code, and added support for the 
	`resize` executable. Hard coded path for Solaris machines.

 Version 2.01, Wed Sep 13 22:22:23 EDT 1995
	Change error reporting around in getscreensize so that if
 	an ioctl fails but getenv succeeds, no warning will be 
	printed. This is an attempt to work around a Solaris bug where
	TIOCGWINSZ fails in telnet sessions.
 
 Version 2.00, Mon Sep  4 06:37:24 EDT 1995
	Added timeouts to select/poll, added USE_STDIO_PTR support
	(required for recent perl revisions), and fixed up compilation
	under OS/2.

 Version 1.99, Fri Aug 11 20:18:11 EDT 1995
	Add file handles to ReadMode.

 Version 1.97, Mon Apr 10 21:41:52 EDT 1995
	Changed mode 5 to disable UC & delays. Added more ECHO flags.
        Tested termio[s] & sgtty.
	Added termoptions so test.pl can give more info.

 Version 1.96,
	Mucked with filehandle selection in ReadKey.pm.

 Version 1.95,
	Cleaning up for distribution.

 Version 1.94,
	Dealt with get/settermsize sillyness.

 Version 1.91, Sat Mar 11 23:47:04 EST 1995:
	Andy's patches, and a bit of termsize finesse.

 Version 1.9, Thu Mar  9 14:11:49 EST 1995:
	Modifying for portability. Prototypes, singed chars, etc.

 Version 1.8, Mon Jan  9 23:18:14 EST 1995:
	Added use of Configure.pm. No changes to ReadKey.

 Version 1.7, Fri Dec 16 13:48:14 EST 1994:
   Getting closer to release. Added new readmode 2. Had to bump up other
   modes, unfortunately. This is the _last_ time I do that. If I have to
   bump up the modes again, I'm switching to a different scheme.

 Version 1.6, Wed Dec 14 17:36:59 EST 1994:
	Completly reorganized the control-char support (twice!) so that
	it is automatically ported by the preproccessor for termio[s], or
	by an included script for sgtty. Logical defaults for sgtty are included
	too. Added Sun TermSize support. (Hope I got it right.)

 Version 1.5, Fri Dec  9 16:07:49 EST 1994:
	Added SetTermSize, GetSpeeds, Get/SetControlChars, PerlIO support.

 Version 1.01, Thu Oct 20 23:32:39 EDT 1994:
	Added Select_fd_set_t casts to select() call.

 Version 1.0: First "real" release. Everything seems cool.


*******************************************************************/

/***

 Things to do:

	Make sure the GetSpeed function is doing it's best to separate ispeed
	from ospeed.
	
	Separate the stty stuff from ReadMode, so that stty -a can be easily
	used, among other things.

***/



/* Using these defines, you can elide anything you know 
   won't work properly */

/* Methods of doing non-blocking reads */

/*#define DONT_USE_SELECT*/
/*#define DONT_USE_POLL*/
/*#define DONT_USE_NODELAY*/


/* Terminal I/O packages */

/*#define DONT_USE_TERMIOS*/
/*#define DONT_USE_TERMIO*/
/*#define DONT_USE_SGTTY*/

/* IOCTLs that can be used for GetTerminalSize */

/*#define DONT_USE_GWINSZ*/
/*#define DONT_USE_GSIZE*/

/* IOCTLs that can be used for SetTerminalSize */

/*#define DONT_USE_SWINSZ*/
/*#define DONT_USE_SSIZE*/


/* This bit is for OS/2 */

#ifdef OS2
#       define I_FCNTL
#       define HAS_FCNTL

#       define O_NODELAY O_NDELAY

#       define DONT_USE_SELECT
#       define DONT_USE_POLL

#       define DONT_USE_TERMIOS
#       define DONT_USE_SGTTY
#       define I_TERMIO
#       define CC_TERMIO

/* This flag should be off in the lflags when we enable termio mode */
#      define TRK_IDEFAULT     IDEFAULT

#       define INCL_SUB
#       define INCL_DOS

#       include <os2.h>
#	include <stdlib.h>

#       define VIOMODE
#else
        /* no os2 */
#endif

/* This bit is for Windows 95/NT */

#ifdef WIN32
#		define DONT_USE_TERMIO
#		define DONT_USE_TERMIOS
#		define DONT_USE_SGTTY
#		define DONT_USE_POLL
#		define DONT_USE_SELECT
#		define DONT_USE_NODELAY
#		define USE_WIN32
#		include <io.h>
#		if defined(_get_osfhandle) && (PERL_VERSION == 4) && (PERL_SUBVERSION < 5)
#			undef _get_osfhandle
#			if defined(_MSC_VER)
#				define level _cnt
#			endif
#		endif
#endif

/* This bit for NeXT */

#ifdef _NEXT_SOURCE
  /* fcntl with O_NDELAY (FNDELAY, actually) is broken on NeXT */
# define DONT_USE_NODELAY
#endif

#if !defined(DONT_USE_NODELAY)
# ifdef HAS_FCNTL
#  define Have_nodelay
#  ifdef I_FCNTL
#   include <fcntl.h>
#  endif
#  ifdef I_SYS_FILE
#   include <sys/file.h>
#  endif
#  ifdef I_UNISTD
#   include <unistd.h>
#  endif

/* If any other headers are needed for fcntl or O_NODELAY, they need to get
   included right here */

#  if !defined(O_NODELAY)
#   if !defined(FNDELAY)
#    undef Have_nodelay
#   else
#    define O_NODELAY FNDELAY
#   endif
#  else
#   define O_NODELAY O_NDELAY
#  endif
# endif
#endif

#if !defined(DONT_USE_SELECT)
# ifdef HAS_SELECT
#  ifdef I_SYS_SELECT
#   include <sys/select.h>
#  endif

/* If any other headers are likely to be needed for select, they need to be
   included right here */

#  define Have_select
# endif
#endif

#if !defined(DONT_USE_POLL)
# ifdef HAS_POLL
#  ifdef HAVE_POLL_H
#   include <poll.h>
#   define Have_poll
#  endif
#  ifdef HAVE_SYS_POLL_H
#   include <sys/poll.h>
#   define Have_poll
#  endif
# endif
#endif

#ifdef DONT_USE_TERMIOS
# ifdef I_TERMIOS
#  undef I_TERMIOS
# endif
#endif
#ifdef DONT_USE_TERMIO
# ifdef I_TERMIO
#  undef I_TERMIO
# endif
#endif
#ifdef DONT_USE_SGTTY
# ifdef I_SGTTY
#  undef I_SGTTY
# endif
#endif

/* Pre-POSIX SVR3 systems sometimes define struct winsize in
   sys/ptem.h.  However, sys/ptem.h needs a type mblk_t (?) which
   is defined in <sys/stream.h>.
   No, Configure (dist3.051) doesn't know how to check for this.
*/
#ifdef I_SYS_STREAM
# include <sys/stream.h>
#endif
#ifdef I_SYS_PTEM
# include <sys/ptem.h>
#endif

#ifdef I_TERMIOS
# include <termios.h>
#else
# ifdef I_TERMIO
#  include <termio.h>
# else
#  ifdef I_SGTTY
#   include <sgtty.h>
#  endif
# endif
#endif

#ifdef I_TERMIOS
# define CC_TERMIOS
#else
# ifdef I_TERMIO
#  define CC_TERMIO
# else
#  ifdef I_SGTTY
#   define CC_SGTTY
#  endif
# endif
#endif

#ifndef TRK_IDEFAULT
/* This flag should be off in the lflags when we enable termio mode */
#      define TRK_IDEFAULT     0
#endif

/* Fix up the disappearance of the '_' macro in Perl 5.7.2 */

#ifndef _
#  ifdef CAN_PROTOTYPE
#    define _(args) args
#  else
#    define _(args) ()
#  endif
#endif

#define DisableFlush (1) /* Should flushing mode changes be enabled?
		            I think not for now. */


#define STDIN PerlIO_stdin()

#include "cchars.h"


STATIC int GetTermSizeVIO _((pTHX_ PerlIO * file,
	int * retwidth, int * retheight, 
	int * xpix, int * ypix));

STATIC int GetTermSizeGWINSZ _((pTHX_ PerlIO * file,
	int * retwidth, int * retheight, 
	int * xpix, int * ypix));

STATIC int GetTermSizeGSIZE _((pTHX_ PerlIO * file,
	int * retwidth, int * retheight, 
	int * xpix, int * ypix));

STATIC int GetTermSizeWin32 _((pTHX_ PerlIO * file,
	int * retwidth, int * retheight,
	int * xpix, int * ypix));

STATIC int SetTerminalSize _((pTHX_ PerlIO * file,
	int width, int height, 
	int xpix, int ypix));

STATIC void ReadMode _((pTHX_ PerlIO * file,int mode));

STATIC int pollfile _((pTHX_ PerlIO * file, double delay));

STATIC int setnodelay _((pTHX_ PerlIO * file, int mode));

STATIC int selectfile _((pTHX_ PerlIO * file, double delay));

STATIC int Win32PeekChar _((pTHX_ PerlIO * file, double delay, char * key));

STATIC int getspeed _((pTHX_ PerlIO * file, I32 *in, I32 * out ));


#ifdef VIOMODE
int GetTermSizeVIO(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	/*int handle=PerlIO_fileno(file);

        static VIOMODEINFO *modeinfo = NULL;

        if (modeinfo == NULL)
                modeinfo = (VIOMODEINFO *)malloc(sizeof(VIOMODEINFO));

        VioGetMode(modeinfo,0);
        *retheight = modeinfo->row ?: 25;
        *retwidth = modeinfo->col ?: 80;*/
	int buf[2];

	_scrsize(&buf[0]);

	*retwidth = buf[0]; *retheight = buf[1];

        *xpix = *ypix = 0;
        return 0;
}
#else
int GetTermSizeVIO(pTHX_ PerlIO *file,int * retwidth,int *retheight, int *xpix,int *ypix)
{
	croak("TermSizeVIO is not implemented on this architecture");
        return 0;
}
#endif


#if defined(TIOCGWINSZ) && !defined(DONT_USE_GWINSZ)
int GetTermSizeGWINSZ(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	int handle=PerlIO_fileno(file);
	struct winsize w;

	if (ioctl (handle, TIOCGWINSZ, &w) == 0) {
		*retwidth=w.ws_col; *retheight=w.ws_row; 
		*xpix=w.ws_xpixel; *ypix=w.ws_ypixel; return 0;
	}
	else {
		return -1; /* failure */
	}

}
#else
int GetTermSizeGWINSZ(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	croak("TermSizeGWINSZ is not implemented on this architecture");
        return 0;
}
#endif

#if (!defined(TIOCGWINSZ) || defined(DONT_USE_GWINSZ)) && (defined(TIOCGSIZE) && !defined(DONT_USE_GSIZE))
int GetTermSizeGSIZE(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	int handle=PerlIO_fileno(file);

	struct ttysize w;

	if (ioctl (handle, TIOCGSIZE, &w) == 0) {
		*retwidth=w.ts_cols; *retheight=w.ts_lines; 
		*xpix=0/*w.ts_xxx*/; *ypix=0/*w.ts_yyy*/; return 0;
	}
	else {
		return -1; /* failure */
	}
}
#else
int GetTermSizeGSIZE(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	croak("TermSizeGSIZE is not implemented on this architecture");
        return 0;
}
#endif

#ifdef USE_WIN32
int GetTermSizeWin32(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	int handle=PerlIO_fileno(file);
	HANDLE whnd = (HANDLE)_get_osfhandle(handle);
	CONSOLE_SCREEN_BUFFER_INFO info;

	if (GetConsoleScreenBufferInfo(whnd, &info)) {
		/* Logic: return maximum possible screen width, but return
		   only currently selected height */
		if (retwidth)
			*retwidth = info.dwMaximumWindowSize.X; 
			/*info.srWindow.Right - info.srWindow.Left;*/
		if (retheight)
			*retheight = info.srWindow.Bottom - info.srWindow.Top;
		if (xpix)
			*xpix = 0;
		if (ypix)
			*ypix = 0;
		return 0;
	} else
		return -1;
}
#else
int GetTermSizeWin32(pTHX_ PerlIO *file,int *retwidth,int *retheight,int *xpix,int *ypix)
{
	croak("TermSizeWin32 is not implemented on this architecture");
        return 0;
}
#endif /* USE_WIN32 */


STATIC int termsizeoptions() {
	return	0
#ifdef VIOMODE
		| 1
#endif
#if defined(TIOCGWINSZ) && !defined(DONT_USE_GWINSZ)
		| 2
#endif
#if defined(TIOCGSIZE) && !defined(DONT_USE_GSIZE)
		| 4
#endif
#if defined(USE_WIN32)
		| 8
#endif
		;
}


int SetTerminalSize(pTHX_ PerlIO *file,int width,int height,int xpix,int ypix)
{
	int handle=PerlIO_fileno(file);

#ifdef VIOMODE
        return -1;
#else

#if defined(TIOCSWINSZ) && !defined(DONT_USE_SWINSZ)
	char buffer[10];
	struct winsize w;

	w.ws_col=width;
	w.ws_row=height;
	w.ws_xpixel=xpix;
	w.ws_ypixel=ypix;
	if (ioctl (handle, TIOCSWINSZ, &w) == 0) {
		sprintf(buffer,"%d",width); /* Be polite to our children */
		my_setenv("COLUMNS",buffer);
		sprintf(buffer,"%d",height);
		my_setenv("LINES",buffer);
		return 0;
	}
	else {
		croak("TIOCSWINSZ ioctl call to set terminal size failed: %s",Strerror(errno));
		return -1;
	}
#else
# if defined(TIOCSSIZE) && !defined(DONT_USE_SSIZE)
	char buffer[10];
	struct ttysize w;

	w.ts_lines=height;
	w.ts_cols=width;
	w.ts_xxx=xpix;
	w.ts_yyy=ypix;
	if (ioctl (handle, TIOCSSIZE, &w) == 0) {
		sprintf(buffer,"%d",width);
		my_setenv("COLUMNS",buffer);
		sprintf(buffer,"%d",height);
		my_setenv("LINES",buffer);
		return 0;
	}
	else {
		croak("TIOCSSIZE ioctl call to set terminal size failed: %s",Strerror(errno));
		return -1;
	}
# else
	/*sprintf(buffer,"%d",width)   * Should we could do this and then *
	my_setenv("COLUMNS",buffer)    * said we succeeded?               *
	sprintf(buffer,"%d",height);
	my_setenv("LINES",buffer)*/

	return -1; /* Fail */
# endif
#endif
#endif

}

STATIC const I32 terminal_speeds[] = {
#ifdef B50
	50, B50,
#endif
#ifdef B75
	75, B75,
#endif
#ifdef B110
	110, B110,
#endif
#ifdef B134
	134, B134,
#endif
#ifdef B150
	150, B150,
#endif
#ifdef B200
	200, B200,
#endif
#ifdef B300
	300, B300,
#endif
#ifdef B600
	600, B600,
#endif
#ifdef B1200
	1200, B1200,
#endif
#ifdef B1800
	1800, B1800,
#endif
#ifdef B2400
	2400, B2400,
#endif
#ifdef B4800
	4800, B4800,
#endif
#ifdef B9600
	9600, B9600,
#endif
#ifdef B19200
	19200, B19200,
#endif
#ifdef B38400
	38400, B38400,
#endif
#ifdef B57600
	57600, B57600,
#endif
#ifdef B115200
	115200, B115200,
#endif
#ifdef EXTA
	19200, EXTA,
#endif
#ifdef EXTB
	38400, EXTB,
#endif
#ifdef B0
	0,  B0,
#endif
	-1,-1
};

int getspeed(pTHX_ PerlIO *file,I32 *in, I32 *out)
{
	int handle=PerlIO_fileno(file);
#if defined(I_TERMIOS) || defined(I_TERMIO) || defined(I_SGTTY)
	int i;
#endif
#       ifdef I_TERMIOS
	/* Posixy stuff */

	struct termios buf;
	tcgetattr(handle,&buf);

	*in = *out = -1;
	*in = cfgetispeed(&buf);
	*out = cfgetospeed(&buf);
	for(i=0;terminal_speeds[i]!=-1;i+=2) {
		if(*in == terminal_speeds[i+1])
			{ *in = terminal_speeds[i]; break; }
	}
	for(i=0;terminal_speeds[i]!=-1;i+=2) {
		if(*out == terminal_speeds[i+1])
			{ *out = terminal_speeds[i]; break; }
	}
	return 0;	 	

#       else
#        ifdef I_TERMIO
	 /* SysV stuff */
	 struct termio buf;

	 ioctl(handle,TCGETA,&buf);

	*in=*out=-1;
	for(i=0;terminal_speeds[i]!=-1;i+=2) {
		if((buf.c_cflag & CBAUD) == terminal_speeds[i+1])
			{ *in=*out=terminal_speeds[i]; break; }
	}
	return 0;	 	

#        else
#         ifdef I_SGTTY
	  /* BSD stuff */
	  struct sgttyb buf;

	  ioctl(handle,TIOCGETP,&buf);

	*in=*out=-1;

	for(i=0;terminal_speeds[i]!=-1;i+=2) 
		if(buf.sg_ospeed == terminal_speeds[i+1])
			{ *out = terminal_speeds[i]; break; }

	for(i=0;terminal_speeds[i]!=-1;i+=2) 
		if(buf.sg_ispeed == terminal_speeds[i+1])
			{ *in = terminal_speeds[i]; break; }

	return 0;	 	


#         else

	   /* No termio, termios or sgtty. I suppose we can try stty,
	      but it would be nice if you could get a better OS */

	return -1;

#         endif
#        endif
#       endif
}

#ifdef WIN32
struct tbuffer { DWORD Mode; };
#else
#ifdef I_TERMIOS
#define USE_TERMIOS
#define tbuffer termios
#else
#ifdef I_TERMIO
#define USE_TERMIO
#define tbuffer termio
#else
#ifdef I_SGTTY
#define USE_SGTTY
struct tbuffer {
	  struct sgttyb buf;
#if defined(TIOCGETC)
	  struct tchars tchar;
#endif
#if defined(TIOCGLTC)
	  struct ltchars ltchar;
#endif
#if defined(TIOCLGET)
	  int local;
#endif
};
#else
#define USE_STTY
struct tbuffer {
	int dummy;
};
#endif
#endif
#endif
#endif

static HV * filehash; /* Used to store the original terminal settings for each handle*/
static HV * modehash; /* Used to record the current terminal "mode" for each handle*/

void ReadMode(pTHX_ PerlIO *file,int mode)
{
	dTHR;
	int handle;
	int firsttime;
	int oldmode;
	struct tbuffer work;
	struct tbuffer	savebuf;

	
	handle=PerlIO_fileno(file);
	
	firsttime=!hv_exists(filehash, (char*)&handle, sizeof(int));


#	ifdef WIN32

	if (!GetConsoleMode((HANDLE)_get_osfhandle(handle), &work.Mode))
	    croak("GetConsoleMode failed, LastError=|%d|",GetLastError());

#	endif /* WIN32 */

#       ifdef USE_TERMIOS
	/* Posixy stuff */
	
	tcgetattr(handle,&work);



#endif
#ifdef USE_TERMIO
	 /* SysV stuff */

	 ioctl(handle,TCGETA,&work);


#endif
#ifdef USE_SGTTY
	  /* BSD stuff */

	  ioctl(handle,TIOCGETP,&work.buf);
# 	  if defined(TIOCGETC)
	   ioctl(handle,TIOCGETC,&work.tchar);
#	  endif
#         if defined(TIOCLGET)
	   ioctl(handle,TIOCLGET,&work.local);
#	  endif
#	  if defined(TIOCGLTC)
	   ioctl(handle,TIOCGLTC,&work.ltchar);
#	  endif


#endif


	if(firsttime) {
		firsttime=0; 
		memcpy((void*)&savebuf,(void*)&work,sizeof(struct tbuffer));
		if(!hv_store(filehash,(char*)&handle,sizeof(int),
			newSVpv((char*)&savebuf,sizeof(struct tbuffer)),0))
			croak("Unable to stash terminal settings.\n");
		if(!hv_store(modehash,(char*)&handle,sizeof(int),newSViv(0),0))
			croak("Unable to stash terminal settings.\n");
	} else {
		SV ** temp;
		if(!(temp=hv_fetch(filehash,(char*)&handle,sizeof(int),0))) 
			croak("Unable to retrieve stashed terminal settings.\n");
		memcpy(&savebuf,SvPV(*temp,PL_na),sizeof(struct tbuffer));
		if(!(temp=hv_fetch(modehash,(char*)&handle,sizeof(int),0))) 
			croak("Unable to retrieve stashed terminal mode.\n");
		oldmode=SvIV(*temp);
	}

#ifdef WIN32

	switch (mode) {
		case 5:
			/* Should 5 disable ENABLE_WRAP_AT_EOL_OUTPUT? */
		case 4:
			work.Mode &= ~(ENABLE_ECHO_INPUT|ENABLE_PROCESSED_INPUT|ENABLE_LINE_INPUT|ENABLE_PROCESSED_OUTPUT);
			work.Mode |= 0;
			break;
		case 3:
			work.Mode &= ~(ENABLE_LINE_INPUT|ENABLE_ECHO_INPUT);
			work.Mode |= ENABLE_PROCESSED_INPUT|ENABLE_PROCESSED_OUTPUT;
			break;
		case 2:
			work.Mode &= ~(ENABLE_ECHO_INPUT);
			work.Mode |= ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT|ENABLE_PROCESSED_OUTPUT;
			break;
		case 1:
			work.Mode &= ~(0);
			work.Mode |= ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT|ENABLE_PROCESSED_OUTPUT;
			break;
		case 0:
			work = savebuf;
			firsttime = 1;
			break;
	}

	if (!SetConsoleMode((HANDLE)_get_osfhandle(handle), work.Mode))
	    croak("SetConsoleMode failed, LastError=|%d|",GetLastError());

#endif /* WIN32 */


#ifdef USE_TERMIOS


/* What, me worry about standards? */

#       if !defined (VMIN)
#		define VMIN VEOF
#       endif

#	if !defined (VTIME)
#		define VTIME VEOL
#	endif

#	if !defined (IXANY)
#		define IXANY (0)
#	endif

#ifndef IEXTEN
#ifdef IDEFAULT
#define IEXTEN IDEFAULT
#endif
#endif

/* XXX Is ONLCR in POSIX?.  The value of '4' seems to be the same for
   both SysV and Sun, so it's probably rather general, and I'm not
   aware of a POSIX way to do this otherwise.
*/
#ifndef ONLCR
# define ONLCR 4
#endif

#ifndef IMAXBEL
#define IMAXBEL 0
#endif
#ifndef ECHOE
#define ECHOE 0
#endif
#ifndef ECHOK
#define ECHOK 0
#endif
#ifndef ECHONL
#define ECHONL 0
#endif 
#ifndef ECHOPRT
#define ECHOPRT 0
#endif
#ifndef FLUSHO
#define FLUSHO 0
#endif
#ifndef PENDIN
#define PENDIN 0
#endif
#ifndef ECHOKE
#define ECHOKE 0
#endif
#ifndef ONLCR
#define ONLCR 0
#endif
#ifndef OCRNL
#define OCRNL 0
#endif
#ifndef ONLRET
#define ONLRET 0
#endif
#ifndef IUCLC
#define IUCLC 0
#endif
#ifndef OPOST
#define OPOST 0
#endif
#ifndef OLCUC
#define OLCUC 0
#endif
#ifndef ECHOCTL
#define ECHOCTL 0
#endif
#ifndef XCASE
#define XCASE 0
#endif
#ifndef BRKINT
#define BRKINT 0
#endif


	if(mode==5) {
		/*\
		 *  Disable everything except parity if needed.
		\*/

		/* Hopefully, this should put the tty into unbuffered mode
		with signals and control characters (both posixy and normal)
		disabled, along with flow control. Echo should be off.
		CR/LF is not translated, along with 8-bit/parity */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_lflag &= ~(ICANON|ISIG|IEXTEN );
		work.c_lflag &= ~(ECHO|ECHOE|ECHOK|ECHONL|ECHOCTL);
		work.c_lflag &= ~(ECHOPRT|ECHOKE|FLUSHO|PENDIN|XCASE);
		work.c_lflag |= NOFLSH;
        work.c_iflag &= ~(IXOFF|IXON|IXANY|ICRNL|IMAXBEL|BRKINT);

		if(((work.c_iflag & INPCK) != INPCK) ||
                   ((work.c_cflag & PARENB) != PARENB)) {
			work.c_iflag &= ~ISTRIP;
			work.c_iflag |= IGNPAR;
			work.c_iflag &= ~PARMRK;
		} 
		work.c_oflag &= ~(OPOST |ONLCR|OCRNL|ONLRET);

		work.c_cc[VTIME] = 0;
		work.c_cc[VMIN] = 1;
	}
	else if(mode==4) {
		/* Hopefully, this should put the tty into unbuffered mode
		with signals and control characters (both posixy and normal)
		disabled, along with flow control. Echo should be off.
		About the only thing left unchanged is 8-bit/parity */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		/*work.c_iflag = savebuf.c_iflag;*/
		work.c_lflag &= ~(ICANON | ISIG | IEXTEN | ECHO);
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL|ECHOCTL|ECHOPRT|ECHOKE);
        work.c_iflag &= ~(IXON | IXANY | BRKINT);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VTIME] = 0;
		work.c_cc[VMIN] = 1;
	}
	else if(mode==3)
	{
		/* This should be an unbuffered mode with signals and control	
		characters enabled, as should be flow control. Echo should
		still be off */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_iflag = savebuf.c_iflag;
		work.c_lflag &= ~(ICANON | ECHO);
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL|ECHOCTL|ECHOPRT|ECHOKE);
		work.c_lflag |= ISIG | IEXTEN;
		/*work.c_iflag &= ~(IXON | IXOFF | IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);
		work.c_oflag = savebuf.c_oflag;*/
		work.c_cc[VTIME] = 0;
		work.c_cc[VMIN] = 1;
	}
	else if(mode==2)
	{
		/* This should be an unbuffered mode with signals and control	
		characters enabled, as should be flow control. Echo should
		still be off */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_iflag = savebuf.c_iflag;
		work.c_lflag |= ICANON|ISIG|IEXTEN;
		work.c_lflag &= ~ECHO;
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL|ECHOCTL|ECHOPRT|ECHOKE);
		/*work.c_iflag &= ~(IXON |IXOFF|IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VTIME] = savebuf.c_cc[VTIME];
		work.c_cc[VMIN] = savebuf.c_cc[VMIN];*/
	}
	else if(mode==1)
	{
		/* This should be an unbuffered mode with signals and control	
		characters enabled, as should be flow control. Echo should
		still be off */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_iflag = savebuf.c_iflag;
		work.c_lflag |= ICANON|ECHO|ISIG|IEXTEN;
		/*work.c_iflag &= ~(IXON |IXOFF|IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VTIME] = savebuf.c_cc[VTIME];
		work.c_cc[VMIN] = savebuf.c_cc[VMIN];*/
	}
	else if(mode==0){
		/*work.c_lflag &= ~BITMASK; 
		work.c_lflag |= savebuf.c_lflag & BITMASK;
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VTIME] = savebuf.c_cc[VTIME];
		work.c_cc[VMIN] = savebuf.c_cc[VMIN];
		work.c_iflag = savebuf.c_iflag;
		work.c_iflag &= ~(IXON|IXOFF|IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);*/
		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));
		/*Copy(&work,&savebuf,1,sizeof(struct tbuffer));*/

		firsttime=1;
	}	
	else
	{
		croak("ReadMode %d is not implemented on this architecture.",mode);
		return;		
	}


	/* If switching from a "lower power" mode to a higher one, keep the
	data that may be in the queue, as it can easily be type-ahead. On
	switching to a lower mode from a higher one, however, flush the queue
	so that raw keystrokes won't hit an unexpecting program */
	
	if(DisableFlush || oldmode<=mode)
		tcsetattr(handle,TCSANOW,&work);
	else
		tcsetattr(handle,TCSAFLUSH,&work);

	/*tcsetattr(handle,TCSANOW,&work);*/ /* It might be better to FLUSH
					   when changing gears to a lower mode,
					   and only use NOW for higher modes. 
					*/


#endif
#ifdef USE_TERMIO

/* What, me worry about standards? */

#	 if !defined (IXANY)
#                define IXANY (0)
#        endif

#ifndef ECHOE
#define ECHOE 0
#endif
#ifndef ECHOK
#define ECHOK 0
#endif
#ifndef ECHONL
#define ECHONL 0
#endif
#ifndef XCASE
#define XCASE 0
#endif
#ifndef BRKINT
#define BRKINT 0
#endif



	 if(mode==5) {
		/* This mode should be echo disabled, signals disabled,
		flow control disabled, and unbuffered. CR/LF translation 
   	 	is off, and 8 bits if possible */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_lflag &= ~(ECHO | ISIG | ICANON | XCASE);
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL | TRK_IDEFAULT);
		work.c_iflag &= ~(IXON | IXOFF | IXANY | ICRNL | BRKINT);
		if((work.c_cflag | PARENB)!=PARENB ) {
			work.c_iflag &= ~(ISTRIP|INPCK);
			work.c_iflag |= IGNPAR;
		} 
		work.c_oflag &= ~(OPOST|ONLCR);
		work.c_cc[VMIN] = 1;
		work.c_cc[VTIME] = 1;
	 } 
	 else if(mode==4) {
		/* This mode should be echo disabled, signals disabled,
		flow control disabled, and unbuffered. Parity is not
		touched. */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_lflag &= ~(ECHO | ISIG | ICANON);
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL TRK_IDEFAULT);
		work.c_iflag = savebuf.c_iflag;
		work.c_iflag &= ~(IXON | IXOFF | IXANY | BRKINT);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VMIN] = 1;
		work.c_cc[VTIME] = 1;
	 } 
	 else if(mode==3) {
		/* This mode tries to have echo off, signals enabled,
		flow control as per the original setting, and unbuffered. */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_lflag &= ~(ECHO | ICANON);
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL | TRK_IDEFAULT);
		work.c_lflag |= ISIG;
		work.c_iflag = savebuf.c_iflag;
		work.c_iflag &= ~(IXON | IXOFF | IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VMIN] = 1;
		work.c_cc[VTIME] = 1;
	 }
	 else if(mode==2) {
		/* This mode tries to set echo on, signals on, and buffering
		on, with flow control set to whatever it was originally. */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_lflag |= (ISIG | ICANON);
		work.c_lflag &= ~ECHO;
		work.c_lflag &= ~(ECHOE | ECHOK | ECHONL | TRK_IDEFAULT);
		work.c_iflag = savebuf.c_iflag;
		work.c_iflag &= ~(IXON | IXOFF | IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VMIN] = savebuf.c_cc[VMIN];
		work.c_cc[VTIME] = savebuf.c_cc[VTIME];
		
		/* This assumes turning ECHO and ICANON back on is
		   sufficient to re-enable cooked mode. If this is a 
		   problem, complain to me */

		/* What the heck. We're already saving the entire buf, so
		I'm now going to reset VMIN and VTIME too. Hope this works 
		properly */
		
	 } 
	 else if(mode==1) {
		/* This mode tries to set echo on, signals on, and buffering
		on, with flow control set to whatever it was originally. */

		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));

		work.c_lflag |= (ECHO | ISIG | ICANON);
      work.c_iflag &= ~TRK_IDEFAULT;
		work.c_iflag = savebuf.c_iflag;
		work.c_iflag &= ~(IXON | IXOFF | IXANY);
		work.c_iflag |= savebuf.c_iflag & (IXON|IXOFF|IXANY);
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VMIN] = savebuf.c_cc[VMIN];
		work.c_cc[VTIME] = savebuf.c_cc[VTIME];
		
		/* This assumes turning ECHO and ICANON back on is
		   sufficient to re-enable cooked mode. If this is a 
		   problem, complain to me */

		/* What the heck. We're already saving the entire buf, so
		I'm now going to reset VMIN and VTIME too. Hope this works 
		properly */
	}		
	 else if(mode==0) {
		/* Put things back the way they were */

		/*work.c_lflag = savebuf.c_lflag;
		work.c_iflag = savebuf.c_iflag;
		work.c_oflag = savebuf.c_oflag;
		work.c_cc[VMIN] = savebuf.c_cc[VMIN];
		work.c_cc[VTIME] = savebuf.c_cc[VTIME];*/
		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));
		firsttime=1;
	 }
 	 else
 	 {
		croak("ReadMode %d is not implemented on this architecture.",mode);
		return;		
	 }


	 if(DisableFlush || oldmode<=mode) 
		ioctl(handle,TCSETA,&work);
	 else
		ioctl(handle,TCSETAF,&work);

#endif
#ifdef USE_SGTTY


	  if(mode==5) {
		/* Unbuffered, echo off, signals off, flow control off */
		/* CR-CR/LF mode off too, and 8-bit path enabled. */
#	 	if defined(TIOCLGET) && defined(LPASS8)
		 if((work.buf.sg_flags & (EVENP|ODDP))==0 ||
		    (work.buf.sg_flags & (EVENP|ODDP))==(EVENP|ODDP))
		 	 work.local |= LPASS8; /* If parity isn't being used, use 8 bits */
#		endif
	  	work.buf.sg_flags &= ~(ECHO|CRMOD);
	  	work.buf.sg_flags |= (RAW|CBREAK);
# 	  	if defined(TIOCGETC)
		 work.tchar.t_intrc = -1;
		 work.tchar.t_quitc = -1;
		 work.tchar.t_startc= -1;
		 work.tchar.t_stopc = -1;
		 work.tchar.t_eofc  = -1;
		 work.tchar.t_brkc  = -1;
#		endif
#		if defined(TIOCGLTC)
		 work.ltchar.t_suspc= -1;
		 work.ltchar.t_dsuspc= -1;
		 work.ltchar.t_rprntc= -1;
		 work.ltchar.t_flushc= -1;
		 work.ltchar.t_werasc= -1;
		 work.ltchar.t_lnextc= -1;
#		endif
	  }
	  else if(mode==4) {
		/* Unbuffered, echo off, signals off, flow control off */
	  	work.buf.sg_flags &= ~(ECHO|RAW);
	  	work.buf.sg_flags |= (CBREAK|CRMOD);
#	 	if defined(TIOCLGET)
		 work.local=savebuf.local;
#		endif
# 	  	if defined(TIOCGETC)
		 work.tchar.t_intrc = -1;
		 work.tchar.t_quitc = -1;
		 work.tchar.t_startc= -1;
		 work.tchar.t_stopc = -1;
		 work.tchar.t_eofc  = -1;
		 work.tchar.t_brkc  = -1;
#		endif
#		if defined(TIOCGLTC)
		 work.ltchar.t_suspc= -1;
		 work.ltchar.t_dsuspc= -1;
		 work.ltchar.t_rprntc= -1;
		 work.ltchar.t_flushc= -1;
		 work.ltchar.t_werasc= -1;
		 work.ltchar.t_lnextc= -1;
#		endif
	  }
	  else if(mode==3) {
		/* Unbuffered, echo off, signals on, flow control on */
		work.buf.sg_flags &= ~(RAW|ECHO);
	  	work.buf.sg_flags |= CBREAK|CRMOD;
#	 	if defined(TIOCLGET)
		 work.local=savebuf.local;
#		endif
#		if defined(TIOCGLTC)
		 work.tchar = savebuf.tchar;
#		endif
#		if defined(TIOCGLTC)
		 work.ltchar = savebuf.ltchar;
#		endif
 	  }
	  else if(mode==2) {
		/* Buffered, echo on, signals on, flow control on */
		work.buf.sg_flags &= ~(RAW|CBREAK);
		work.buf.sg_flags |= CRMOD;
		work.buf.sg_flags &= ~ECHO;
#	 	if defined(TIOCLGET)
		 work.local=savebuf.local;
#		endif
#		if defined(TIOCGLTC)
		 work.tchar = savebuf.tchar;
#		endif
#		if defined(TIOCGLTC)
		 work.ltchar = savebuf.ltchar;
#		endif
	  }
	  else if(mode==1) {
		/* Buffered, echo on, signals on, flow control on */
		work.buf.sg_flags &= ~(RAW|CBREAK);
		work.buf.sg_flags |= ECHO|CRMOD;
#	 	if defined(TIOCLGET)
		 work.local=savebuf.local;
#		endif
#		if defined(TIOCGLTC)
		 work.tchar = savebuf.tchar;
#		endif
#		if defined(TIOCGLTC)
		 work.ltchar = savebuf.ltchar;
#		endif
	  }
	  else if(mode==0){
		/* Original settings */
#if 0
		work.buf.sg_flags &= ~(RAW|CBREAK|ECHO|CRMOD);
		work.buf.sg_flags |= savebuf.sg_flags & (RAW|CBREAK|ECHO|CRMOD);
#	 	if defined(TIOCLGET)
		 work.local=savebuf.local;
#		endif
#		if defined(TIOCGLTC)
		 work.tchar = savebuf.tchar;
#		endif
#		if defined(TIOCGLTC)
		 work.ltchar = savebuf.ltchar;
#		endif
#endif
		memcpy((void*)&work,(void*)&savebuf,sizeof(struct tbuffer));
		firsttime=1;
	  }
 	  else
 	  {
		croak("ReadMode %d is not implemented on this architecture.",mode);
		return;		
	  }
#if defined(TIOCLSET)
	  ioctl(handle,TIOCLSET,&work.local);
#endif
#if defined(TIOCSETC)
	  ioctl(handle,TIOCSETC,&work.tchar);
#endif
#	  if defined(TIOCGLTC)
	   ioctl(handle,TIOCSLTC,&work.ltchar);
#	  endif
	  if(DisableFlush || oldmode<=mode)
	  	ioctl(handle,TIOCSETN,&work.buf);
	  else
		ioctl(handle,TIOCSETP,&work.buf);
#endif
#ifdef USE_STTY

	   /* No termio, termios or sgtty. I suppose we can try stty,
	      but it would be nice if you could get a better OS */

	   if(mode==5)
		system("/bin/stty  raw -cbreak -isig -echo -ixon -onlcr -icrnl -brkint");
	   else if(mode==4)
		system("/bin/stty -raw  cbreak -isig -echo -ixon  onlcr  icrnl -brkint");
	   else if(mode==3)
		system("/bin/stty -raw  cbreak  isig -echo  ixon  onlcr  icrnl  brkint");
	   else if(mode==2) 
		system("/bin/stty -raw -cbreak  isig  echo  ixon  onlcr  icrnl  brkint");
	   else if(mode==1)
		system("/bin/stty -raw -cbreak  isig -echo  ixon  onlcr  icrnl  brkint");
	   else if(mode==0)
		system("/bin/stty -raw -cbreak  isig  echo  ixon  onlcr  icrnl  brkint");

	   /* Those probably won't work, but they couldn't hurt 
              at this point */

#endif

	/*warn("Mode set to %d.\n",mode);*/

	if( firsttime ) {
		(void)hv_delete(filehash,(char*)&handle,sizeof(int),0);
		(void)hv_delete(modehash,(char*)&handle,sizeof(int),0);
	} else {
		if(!hv_store(modehash,(char*)&handle,sizeof(int),
			newSViv(mode),0))
			croak("Unable to stash terminal settings.\n");
	}

}

#ifdef USE_PERLIO

/* Make use of a recent addition to Perl, if possible */
# define FCOUNT(f) PerlIO_get_cnt(f)
#else

 /* Make use of a recent addition to Configure, if possible */
# ifdef USE_STDIO_PTR
#  define FCOUNT(f) PerlIO_get_cnt(f)
# else
  /* This bit borrowed from pp_sys.c. Complain to Larry if it's broken. */
  /* If any of this works PerlIO_get_cnt() will too ... NI-S */
#  if defined(USE_STD_STDIO) || defined(atarist) /* this will work with atariST */
#   define FBASE(f) ((f)->_base)
#   define FSIZE(f) ((f)->_cnt + ((f)->_ptr - (f)->_base))
#   define FPTR(f) ((f)->_ptr)
#   define FCOUNT(f) ((f)->_cnt)
#  else
#   if defined(USE_LINUX_STDIO)
#     define FBASE(f) ((f)->_IO_read_base)
#     define FSIZE(f) ((f)->_IO_read_end - FBASE(f))
#     define FPTR(f) ((f)->_IO_read_ptr)
#     define FCOUNT(f) ((f)->_IO_read_end - FPTR(f))
#   endif
#  endif
# endif
#endif

/* This is for the best, I'm afraid. */
#if !defined(FCOUNT)
# ifdef Have_select
#  undef Have_select
# endif
# ifdef Have_poll
#  undef Have_poll
# endif
#endif

/* Note! If your machine has a bolixed up select() call that doesn't
understand this syntax, either fix the checkwaiting call below, or define
DONT_USE_SELECT. */

#ifdef Have_select
int selectfile(pTHX_ PerlIO *file,double delay)
{
	struct timeval t;
	int handle=PerlIO_fileno(file);

	/*char buf[32];    
	Select_fd_set_t fd=(Select_fd_set_t)&buf[0];*/

	fd_set fd;
	if (PerlIO_fast_gets(file) && PerlIO_get_cnt(file) > 0)
		return 1;

	/*t.tv_sec=t.tv_usec=0;*/

        if (delay < 0.0)
            delay = 0.0;
        t.tv_sec = (long)delay;
        delay -= (double)t.tv_sec;
        t.tv_usec = (long)(delay * 1000000.0);

	FD_ZERO(&fd);
	FD_SET(handle,&fd);
	if(select(handle+1,(Select_fd_set_t)&fd,
			   (Select_fd_set_t)0,
			   (Select_fd_set_t)&fd, &t)) return -1; 
	else return 0;
}

#else
int selectfile(pTHX_ PerlIO *file, double delay)
{
	croak("select is not supported on this architecture");
	return 0;
}
#endif

#ifdef Have_nodelay
int setnodelay(pTHX_ PerlIO *file, int mode)
{
	int handle=PerlIO_fileno(file);
	int flags;
	flags=fcntl(handle,F_GETFL,0);
	if(mode)
		flags|=O_NODELAY;
	else
		flags&=~O_NODELAY;
	fcntl(handle,F_SETFL,flags);
	return 0;
}

#else
int setnodelay(pTHX_ PerlIO *file, int mode)
{
	croak("setnodelay is not supported on this architecture");
	return 0;
}
#endif

#ifdef Have_poll
int pollfile(pTHX_ pTHX_ PerlIO *file,double delay)
{
	int handle=PerlIO_fileno(file);
	struct pollfd fds;
	if (PerlIO_fast_gets(f) && PerlIO_get_cnt(f) > 0)
		return 1;
	if(delay<0.0) delay = 0.0;
	fds.fd=handle;
	fds.events=POLLIN;
	fds.revents=0;
	return (poll(&fds,1,(long)(delay * 1000.0))>0);
} 
#else
int pollfile(pTHX_ PerlIO *file,double delay)
{
	croak("pollfile is not supported on this architecture");
	return 0;
}
#endif

#ifdef WIN32

/*

 This portion of the Win32 code is partially borrowed from a version of PDCurses.

*/

typedef struct {
    int repeatCount;
    int vKey;
    int vScan;
    int ascii;
    int control;
} win32_key_event_t;

#define KEY_PUSH(I, K) { events[I].repeatCount = 1; events[I].ascii = K; }
#define KEY_PUSH3(K1, K2, K3) \
    do { \
             eventCount = 0;            \
             KEY_PUSH(2, K1);           \
             KEY_PUSH(1, K2);           \
             KEY_PUSH(0, K3);           \
             eventCount = 3;            \
             goto again;                \
    } while (0)

#define KEY_PUSH4(K1, K2, K3, K4) \
    do { \
             eventCount = 0;            \
             KEY_PUSH(3, K1);           \
             KEY_PUSH(2, K2);           \
             KEY_PUSH(1, K3);           \
             KEY_PUSH(0, K4);           \
             eventCount = 4;            \
             goto again;                \
    } while (0)

int Win32PeekChar(pTHX_ PerlIO *file,double delay,char *key)
{
	int handle;
	HANDLE whnd;
	INPUT_RECORD record;
	DWORD readRecords;

#if 0
	static int keyCount = 0;
	static char lastKey = 0;
#endif

#define MAX_EVENTS 4
    static int eventCount = 0;
    static win32_key_event_t events[MAX_EVENTS];
    int keyCount;

	file = STDIN;

	handle = PerlIO_fileno(file);
	whnd = /*GetStdHandle(STD_INPUT_HANDLE)*/(HANDLE)_get_osfhandle(handle);


again:
#if 0
	if (keyCount > 0) {
		keyCount--;
		*key = lastKey;
	    return TRUE;
	}
#endif

    /* printf("eventCount: %d\n", eventCount); */
    if (eventCount) {
        /* printf("key %d; repeatCount %d\n", *key, events[eventCount - 1].repeatCount); */
        *key = events[eventCount - 1].ascii;
        events[eventCount - 1].repeatCount--;
        if (events[eventCount - 1].repeatCount <= 0) {
            eventCount--;
        }
        return TRUE;
    }

	if (delay > 0) {
		if (WaitForSingleObject(whnd, delay * 1000) != WAIT_OBJECT_0)
		{
			return FALSE;
		}
	}

	if (delay != 0) {
		PeekConsoleInput(whnd, &record, 1, &readRecords);
		if (readRecords == 0) {
			return(FALSE);
        }
	}

	ReadConsoleInput(whnd, &record, 1, &readRecords);
	switch(record.EventType)
   {
    case KEY_EVENT:
		/* printf("\nkeyDown = %d, repeat = %d, vKey = %d, vScan = %d, ASCII = %d, Control = %d\n",
			record.Event.KeyEvent.bKeyDown,
			record.Event.KeyEvent.wRepeatCount,
			record.Event.KeyEvent.wVirtualKeyCode,
			record.Event.KeyEvent.wVirtualScanCode,
			record.Event.KeyEvent.uChar.AsciiChar,
			record.Event.KeyEvent.dwControlKeyState); */

         if (record.Event.KeyEvent.bKeyDown == FALSE)
            goto again;                        /* throw away KeyUp events */

         if (record.Event.KeyEvent.wVirtualKeyCode == 38) { /* up */
             KEY_PUSH3(27, 91, 65);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 40) { /* down */
             KEY_PUSH3(27, 91, 66);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 39) { /* right */
             KEY_PUSH3(27, 91, 67);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 37) { /* left */
             KEY_PUSH3(27, 91, 68);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 33) { /* page up */
             KEY_PUSH3(27, 79, 121);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 34) { /* page down */
             KEY_PUSH3(27, 79, 115);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 36) { /* home */
             KEY_PUSH4(27, 91, 49, 126);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 35) { /* end */
             KEY_PUSH4(27, 91, 52, 126);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 45) { /* insert */
             KEY_PUSH4(27, 91, 50, 126);
         }
         if (record.Event.KeyEvent.wVirtualKeyCode == 46) { /* delete */
             KEY_PUSH4(27, 91, 51, 126);
         }

         if (record.Event.KeyEvent.wVirtualKeyCode == 16
         ||  record.Event.KeyEvent.wVirtualKeyCode == 17
         ||  record.Event.KeyEvent.wVirtualKeyCode == 18
         ||  record.Event.KeyEvent.wVirtualKeyCode == 20
         ||  record.Event.KeyEvent.wVirtualKeyCode == 144
         ||  record.Event.KeyEvent.wVirtualKeyCode == 145)
            goto again;  /* throw away shift/alt/ctrl key only key events */
         keyCount = record.Event.KeyEvent.wRepeatCount;
		 break;
    default:
         keyCount = 0;
         goto again;
         break;
   }

 *key = record.Event.KeyEvent.uChar.AsciiChar; 
 keyCount--;

 if (keyCount) {
     events[0].repeatCount = keyCount;
     events[0].ascii = *key;
     eventCount = 1;
 }
 
 return(TRUE);

 /* again:
	return (FALSE);
	*/


} 
#else
int Win32PeekChar(pTHX_ PerlIO *file, double delay,char *key)
{
	croak("Win32PeekChar is not supported on this architecture");
	return 0;
}
#endif


STATIC int blockoptions() {
	return	0
#ifdef Have_nodelay
		| 1
#endif
#ifdef Have_poll
		| 2
#endif
#ifdef Have_select
		| 4
#endif
#ifdef USE_WIN32
		| 8
#endif
		;
}

STATIC int termoptions() {
	int i=0;
#ifdef USE_TERMIOS
	i=1;		
#endif
#ifdef USE_TERMIO
	i=2;
#endif
#ifdef USE_SGTTY
	i=3;
#endif
#ifdef USE_STTY
	i=4;
#endif
#ifdef USE_WIN32
	i=5;
#endif
	return i;
}



MODULE = Term::ReadKey		PACKAGE = Term::ReadKey

int
selectfile(file,delay)
	InputStream	file
	double	delay
CODE:
	RETVAL = selectfile(aTHX_ file, delay);
OUTPUT:
	RETVAL

# Clever, eh?
void
SetReadMode(mode,file=STDIN)
	int	mode
	InputStream	file
	CODE:
	{
		ReadMode(aTHX_ file,mode);
	}

int
setnodelay(file,mode)
	InputStream	file
	int	mode
CODE:
	RETVAL = setnodelay(aTHX_ file, mode);
OUTPUT:
	RETVAL

int
pollfile(file,delay)
	InputStream	file
	double	delay
CODE:
	RETVAL = pollfile(aTHX_ file, delay);
OUTPUT:
	RETVAL

SV *
Win32PeekChar(file, delay)
	InputStream	file
	double	delay
	CODE:
	{
		char key;
		if (Win32PeekChar(aTHX_ file, delay, &key))
			RETVAL = newSVpv(&key, 1);
		else
			RETVAL = newSVsv(&PL_sv_undef);
	}
	OUTPUT:
	RETVAL

int
blockoptions()

int
termoptions()

int
termsizeoptions()

void
GetTermSizeWin32(file=STDIN)
	InputStream	file
	PPCODE:
	{
		int x,y,xpix,ypix;
		if( GetTermSizeWin32(aTHX_ file,&x,&y,&xpix,&ypix)==0)
		{
			EXTEND(sp, 4);
			PUSHs(sv_2mortal(newSViv((IV)x)));
			PUSHs(sv_2mortal(newSViv((IV)y)));
			PUSHs(sv_2mortal(newSViv((IV)xpix)));
			PUSHs(sv_2mortal(newSViv((IV)ypix)));
		}
		else
		{
			ST(0) = sv_newmortal();
		}
	}

void
GetTermSizeVIO(file=STDIN)
	InputStream	file
	PPCODE:
	{
		int x,y,xpix,ypix;
		if( GetTermSizeVIO(aTHX_ file,&x,&y,&xpix,&ypix)==0)
		{
			EXTEND(sp, 4);
			PUSHs(sv_2mortal(newSViv((IV)x)));
			PUSHs(sv_2mortal(newSViv((IV)y)));
			PUSHs(sv_2mortal(newSViv((IV)xpix)));
			PUSHs(sv_2mortal(newSViv((IV)ypix)));
		}
		else
		{
			ST(0) = sv_newmortal();
		}
	}

void
GetTermSizeGWINSZ(file=STDIN)
	InputStream	file
	PPCODE:
	{
		int x,y,xpix,ypix;
		if( GetTermSizeGWINSZ(aTHX_ file,&x,&y,&xpix,&ypix)==0)
		{
			EXTEND(sp, 4);
			PUSHs(sv_2mortal(newSViv((IV)x)));
			PUSHs(sv_2mortal(newSViv((IV)y)));
			PUSHs(sv_2mortal(newSViv((IV)xpix)));
			PUSHs(sv_2mortal(newSViv((IV)ypix)));
		}
		else
		{
			ST(0) = sv_newmortal();
		}
	}

void
GetTermSizeGSIZE(file=STDIN)
	InputStream	file
	PPCODE:
	{
		int x,y,xpix,ypix;
		if( GetTermSizeGSIZE(aTHX_ file,&x,&y,&xpix,&ypix)==0)
		{
			EXTEND(sp, 4);
			PUSHs(sv_2mortal(newSViv((IV)x)));
			PUSHs(sv_2mortal(newSViv((IV)y)));
			PUSHs(sv_2mortal(newSViv((IV)xpix)));
			PUSHs(sv_2mortal(newSViv((IV)ypix)));
		}
		else
		{
			ST(0) = sv_newmortal();
		}
	}

int
SetTerminalSize(width,height,xpix,ypix,file=STDIN)
	int	width
	int	height
	int	xpix
	int	ypix
	InputStream	file
	CODE:
	{
		RETVAL=SetTerminalSize(aTHX_ file,width,height,xpix,ypix);
	}
	OUTPUT:
		RETVAL

void
GetSpeed(file=STDIN)
	InputStream	file
	PPCODE:
	{
		I32 in,out;
/*
 *    experimentally relaxed for 
 *    https://rt.cpan.org/Ticket/Display.html?id=88050
		if(items!=0) {
			croak("Usage: Term::ReadKey::GetSpeed()");
		}
*/
		if(getspeed(aTHX_ file,&in,&out)) {
			/* Failure */
			ST( 0) = sv_newmortal();
		} else {
			EXTEND(sp, 2);
			PUSHs(sv_2mortal(newSViv((IV)in)));
			PUSHs(sv_2mortal(newSViv((IV)out)));
		}
	}



BOOT: 
newXS("Term::ReadKey::GetControlChars", XS_Term__ReadKey_GetControlChars, file);
newXS("Term::ReadKey::SetControlChars", XS_Term__ReadKey_SetControlChars, file);
filehash=newHV();
modehash=newHV();
