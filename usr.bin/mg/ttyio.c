/*
 * Name:	MicroEMACS
 *		System V terminal I/O.
 * Version:	0
 * Last edit:	Tue Aug 26 23:57:57 PDT 1986
 * By:		gonzo!daveb
 *		{sun, amdahl, mtxinu}!rtech!gonzo!daveb
 *
 * The functions in this file
 * negotiate with the operating system for
 * keyboard characters, and write characters to
 * the display in a barely buffered fashion.
 *
 * This version goes along with tty/termcap/tty.c.
 * Terminal size is determined there, rather than here, and
 * this does not open the termcap file
 */
#include	"def.h"

#include	<sys/types.h>
#include	<fcntl.h>
#include	<termios.h>

#define	NOBUF	512			/* Output buffer size.		*/

char	obuf[NOBUF];			/* Output buffer.		*/
int	nobuf;				/* buffer count			*/

static struct termios	ot;		/* entry state of the terminal	*/
static struct termios	nt;		/* editor's terminal state	*/

static int ttyactivep = FALSE;		/* terminal in editor mode?	*/
static int ttysavedp = FALSE;		/* terminal state saved?	*/

int	nrow;				/* Terminal size, rows.		*/
int	ncol;				/* Terminal size, columns.	*/

/* These are used to implement typeahead on System V */

int kbdflgs;			/* saved keyboard fd flags	*/
int kbdpoll;			/* in O_NDELAY mode			*/
int kbdqp;			/* there is a char in kbdq	*/
char kbdq;			/* char we've already read	*/

/*
 * This function gets called once, to set up
 * the terminal channel.  This version turns off flow
 * control.  This may be wrong for your system, but no
 * good solution has really been found (daveb).
 */
ttopen()
{
	register char	*cp;
	extern char	*getenv();

	if (ttyactivep)
		return;

	if( !ttysavedp )
	{
		if (tcgetattr(0, &ot) < 0)
			abort();
		nt = ot;		/* save entry state		*/
		nt.c_cc[VMIN] = 1;	/* one character read is OK	*/
		nt.c_cc[VTIME] = 0;	/* Never time out.		*/
		nt.c_iflag |= IGNBRK;
		nt.c_iflag &= ~( ICRNL | INLCR | ISTRIP | IXON | IXOFF );
		nt.c_oflag &= ~OPOST;
		nt.c_cflag |= CS8;	/* allow 8th bit on input	*/
		nt.c_cflag &= ~PARENB;	/* Don't check parity		*/
		nt.c_lflag &= ~( ECHO | ICANON | ISIG );

		kbdpoll = (((kbdflgs = fcntl(0, F_GETFL, 0)) & O_NDELAY) != 0);
	
		ttysavedp = TRUE;
	}
	
	if (tcsetattr(0, TCSAFLUSH, &nt) < 0)
		abort();

	/* This really belongs in tty/termcap... */

	if ((cp=getenv("TERMCAP")) == NULL
	|| (nrow=getvalue(cp, "li")) <= 0
	|| (ncol=getvalue(cp, "co")) <= 0) {
		nrow = 24;
		ncol = 80;
	}
	if (nrow > NROW)			/* Don't crash if the	*/
		nrow = NROW;			/* termcap entry is	*/
	if (ncol > NCOL)			/* too big.		*/
		ncol = NCOL;

	ttyactivep = TRUE;
}

/*
 * This routine scans a string, which is
 * actually the return value of a getenv call for the TERMCAP
 * variable, looking for numeric parameter "name". Return the value
 * if found. Return -1 if not there. Assume that "name" is 2
 * characters long. This limited use of the TERMCAP lets us find
 * out the size of a window on the X display.
 */
getvalue(cp, name)
register char	*cp;
register char	*name;
{
	for (;;) {
		while (*cp!=0 && *cp!=':')
			++cp;
		if (*cp++ == 0)			/* Not found.		*/
			return (-1);
		if (cp[0]==name[0] && cp[1]==name[1] && cp[2]=='#')
			return (atoi(cp+3));	/* Stops on ":".	*/
	}
}

/*
 * This function gets called just
 * before we go back home to the shell. Put all of
 * the terminal parameters back.
 */
ttclose()
{
	if(!ttysavedp || !ttyactivep)
		return;
	ttflush();
	if (tcsetattr(0, TCSAFLUSH, &ot) < 0 ||
	    fcntl( 0, F_SETFL, kbdflgs ) < 0)
		abort();
	ttyactivep = FALSE;
}

/*
 * Write character to the display.
 * Characters are buffered up, to make things
 * a little bit more efficient.
 */
ttputc(c)
{
	if (nobuf >= NOBUF)
		ttflush();
	obuf[nobuf++] = c;
}

/*
 * Flush output.
 */
ttflush()
{
	if (nobuf != 0) {
		write(1, obuf, nobuf);
		nobuf = 0;
	}
}

/*
 * Read character from terminal.
 * All 8 bits are returned, so that you can use
 * a multi-national terminal.
 *
 * If keyboard 'queue' already has typeahead from a typeahead() call,
 * just return it.  Otherwise, make sure we are in blocking i/o mode
 * and read a character.
 */
ttgetc()
{
	if( kbdqp )
		kbdqp = FALSE;
	else
	{
		if( kbdpoll && fcntl( 0, F_SETFL, kbdflgs ) < 0 )
			abort();
		kbdpoll = FALSE;
		while (read(0, &kbdq, 1) != 1)
			;
	}
	return ( kbdq & 0xff );
}

/*
 * Return non-FALSE if typeahead is pending.
 *
 * If already got unread typeahead, do nothing.
 * Otherwise, set keyboard to O_NDELAY if not already, and try
 * a one character read.
 */
typeahead()
{
	if( !kbdqp )
	{
		if( !kbdpoll && fcntl( 0, F_SETFL, kbdflgs | O_NDELAY ) < 0 )
			abort();
		kbdpoll = TRUE;
		kbdqp = (1 == read( 0, &kbdq, 1 ));
	}
	return ( kbdqp );
}


/*
 * panic:  print error and die, leaving core file.
 * Don't know why this is needed (daveb).
 */
panic(s)
char *s;
{
	fprintf(stderr, "%s\r\n", s);
	abort();
}


/*
** This should check the size of the window, and reset if needed.
*/

setttysize()
{
#ifdef	TIOCGWINSZ
        struct winsize winsize;
	if (ioctl(0, TIOCGWINSZ, (char *) &winsize) == 0) {
		nrow = winsize . ws_row;
		ncol = winsize . ws_col;
	} else
#endif
	if ((nrow=tgetnum ("li")) <= 0
	|| (ncol=tgetnum ("co")) <= 0) {
		nrow = 24;
		ncol = 80;
	}
	if (nrow > NROW)			/* Don't crash if the	*/
		nrow = NROW;			/* termcap entry is	*/
	if (ncol > NCOL)			/* too big.		*/
		ncol = NCOL;
}

#ifndef NO_DPROMPT
#include <signal.h>
#include <setjmp.h>

static jmp_buf tohere;

static void alrm()
{
	longjmp(tohere, -1);
}

/*
 * Return TRUE if we wait without doing anything, else return FALSE.
 */

ttwait()
{
	if (kbdqp)
		return FALSE;		/* already pending input	*/
	if (setjmp(tohere))
		return TRUE;		/* timeout on read if here	*/
	signal(SIGALRM, alrm); alarm(2);
	kbdqp = (1 == read(0, &kbdq, 1));
	alarm(0);
	return FALSE;			/* successful read if here	*/
}
#endif NO_DPROMPT
