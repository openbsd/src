/*
 * POSIX terminal I/O.
 *
 * The functions in this file
 * negotiate with the operating system for
 * keyboard characters, and write characters to
 * the display in a barely buffered fashion.
 */
#include	"def.h"

#include	<sys/types.h>
#include	<sys/time.h>
#include	<sys/ioctl.h>
#include	<fcntl.h>
#include	<termios.h>
#include	<term.h>

#define	NOBUF	512			/* Output buffer size.		*/

#ifndef TCSASOFT
#define TCSASOFT	0
#endif

char	obuf[NOBUF];			/* Output buffer.		*/
int	nobuf;				/* buffer count			*/

static struct termios	ot;		/* entry state of the terminal	*/
static struct termios	nt;		/* editor's terminal state	*/

static int ttyactivep = FALSE;		/* terminal in editor mode?	*/
static int ttysavedp = FALSE;		/* terminal state saved?	*/

int	nrow;				/* Terminal size, rows.		*/
int	ncol;				/* Terminal size, columns.	*/

/*
 * This function gets called once, to set up
 * the terminal channel.  This version turns off flow
 * control.  This may be wrong for your system, but no
 * good solution has really been found (daveb).
 */
ttopen()
{
	register char	*cp;

	if (ttyactivep)
		return;

	if( !ttysavedp )
	{
		if (tcgetattr(0, &ot) < 0)
			abort();
		nt = ot;		/* save entry state		*/
		/* Set terminal to 'raw' mode and ignore a 'break' */
		nt.c_cc[VMIN] = 1;
		nt.c_cc[VTIME] = 0;
		nt.c_iflag |= IGNBRK;
		nt.c_iflag &= ~(BRKINT|PARMRK|INLCR|IGNCR|ICRNL|IXON);
		nt.c_oflag &= ~OPOST;
		nt.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);

#if !TCSASOFT
		/*
		 * If we don't have TCSASOFT, force terminal to
		 * 8 bits, no parity.
		 */
		nt.c_iflag &= ~ISTRIP;
		nt.c_cflag &= ~(CSIZE|PARENB);
		nt.c_cflag |= CS8;
#endif

		ttysavedp = TRUE;
	}
	
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &nt) < 0)
		abort();

	ttyactivep = TRUE;
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
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &ot) < 0)
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
	int    c;

	while (read(0, &c, 1) != 1)
		;
	return (c & 0xFF);
}

/*
 * Return TRUE if there are characters waiting to be read.
 */
typeahead()
{
	int x;

	return((ioctl(0, FIONREAD, (char *) &x) < 0) ? 0 : x);
}


/*
 * panic:  print error and die, leaving core file.
 * Don't know why this is needed (daveb).
 */
panic(s)
char *s;
{

	(void) fputs("panic: ", stderr);
	(void) fputs(s, stderr);
	(void) fputc('\n', stderr);
	abort();		/* To leave a core image. */
}


/*
** This should check the size of the window, and reset if needed.
*/

setttysize()
{
#ifdef	TIOCGWINSZ
        struct winsize winsize;

	if (ioctl(0, TIOCGWINSZ, (char *) &winsize) == 0) {
		nrow = winsize.ws_row;
		ncol = winsize.ws_col;
	} else
#endif
	if ((nrow = lines) <= 0 || (ncol = columns) <= 0) {
		nrow = 24;
		ncol = 80;
	}

	/* Enforce maximum screen size. */
	if (nrow > NROW)
		nrow = NROW;
	if (ncol > NCOL)
		ncol = NCOL;
}

#ifndef NO_DPROMPT
/*
 * Return TRUE if we wait without doing anything, else return FALSE.
 */
ttwait()
{
	fd_set readfds;
	struct timeval tmout;

	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	tmout.tv_sec = 2;
	tmout.tv_usec = 0;

	if ((select(1, &readfds, NULL, NULL, &tmout)) == 0)
		return(TRUE);
	return(FALSE);
}
#endif NO_DPROMPT
