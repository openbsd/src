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

#define NOBUF	512			/* Output buffer size.		*/

#ifndef TCSASOFT
#define TCSASOFT	0
#endif

char	obuf[NOBUF];			/* Output buffer.		*/
int	nobuf;				/* Buffer count.		*/
struct	termios	oldtty;			/* POSIX tty settings.		*/
struct	termios	newtty;
int	nrow;				/* Terminal size, rows.		*/
int	ncol;				/* Terminal size, columns.	*/

/* XXX - move most of these to def.h? */
void ttopen __P((void));
int ttraw __P((void));
void ttclose __P((void));
int ttcooked __P((void));
void ttputc __P((int));
void ttflush __P((void));
int ttgetc __P((void));
void setttysize __P((void));
int typeahead __P((void));
void panic __P((char *));
int ttwait __P((void));

/*
 * This function gets called once, to set up the terminal.
 * On systems w/o TCSASOFT we turn off off flow control,
 * which isn't really the right thing to do.
 */
void
ttopen()
{

	if (ttraw() == FALSE)
		panic("aborting due to terminal initialize failure");
}

/*
 * This function sets the terminal to RAW mode, as defined for the current
 * shell.  This is called both by ttopen() above and by spawncli() to
 * get the current terminal settings and then change them to what
 * mg expects.	Thus, tty changes done while spawncli() is in effect
 * will be reflected in mg.
 */
int
ttraw()
{

	if (tcgetattr(0, &oldtty) < 0) {
		ewprintf("ttopen can't get terminal attributes");
		return(FALSE);
	}
	(void)memcpy(&newtty, &oldtty, sizeof(newtty));
	/* Set terminal to 'raw' mode and ignore a 'break' */
	newtty.c_cc[VMIN] = 1;
	newtty.c_cc[VTIME] = 0;
	newtty.c_iflag |= IGNBRK;
	newtty.c_iflag &= ~(BRKINT | PARMRK | INLCR | IGNCR | ICRNL | IXON);
	newtty.c_oflag &= ~OPOST;
	newtty.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);

#if !TCSASOFT
	/*
	 * If we don't have TCSASOFT, force terminal to
	 * 8 bits, no parity.
	 */
	newtty.c_iflag &= ~ISTRIP;
	newtty.c_cflag &= ~(CSIZE | PARENB);
	newtty.c_cflag |= CS8;
#endif
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &newtty) < 0) {
		ewprintf("ttopen can't tcsetattr");
		return(FALSE);
	}
	return(TRUE);
}

/*
 * This function gets called just before we go back home to the shell.
 * Put all of the terminal parameters back.
 * Under UN*X this just calls ttcooked(), but the ttclose() hook is in
 * because vttidy() in display.c expects it for portability reasons.
 */
void
ttclose()
{

	if (ttcooked() == FALSE)
		panic("");		/* ttcooked() already printf'd */
}

/*
 * This function restores all terminal settings to their default values,
 * in anticipation of exiting or suspending the editor.
 */
int
ttcooked()
{

	ttflush();
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &oldtty) < 0) {
		ewprintf("ttclose can't tcsetattr");
		return(FALSE);
	}
	return(TRUE);
}

/*
 * Write character to the display.  Characters are buffered up,
 * to make things a little bit more efficient.
 */
void
ttputc(c)
	int c;
{

	if (nobuf >= NOBUF)
		ttflush();
	obuf[nobuf++] = c;
}

/*
 * Flush output.
 */
void
ttflush()
{

	if (nobuf != 0) {
		if (write(1, obuf, nobuf) != nobuf)
			panic("ttflush write failed");
		nobuf = 0;
	}
}

/*
 * Read character from terminal.
 * All 8 bits are returned, so that you can use
 * a multi-national terminal.
 */
int
ttgetc()
{
	char	c;

	while (read(0, &c, 1) != 1)
		;
	return ((int) c);
}

/*
 * Set the tty size.
 * XXX - belongs in tty.c since it uses terminfo vars.
 */
void
setttysize()
{
#ifdef	TIOCGWINSZ
	struct	winsize winsize;

	if (ioctl(0, TIOCGWINSZ, (char *) &winsize) == 0) {
		nrow = winsize.ws_row;
		ncol = winsize.ws_col;
	} else nrow = 0;
#endif
	if ((nrow <= 0 || ncol <= 0) &&
	    ((nrow = lines) <= 0 || (ncol = columns) <= 0)) {
		nrow = 24;
		ncol = 80;
	}

	/* Enforce maximum screen size. */
	if (nrow > NROW)
		nrow = NROW;
	if (ncol > NCOL)
		ncol = NCOL;
}

/*
 * Returns TRUE if there are characters waiting to be read.
 */
int
typeahead()
{
	int	x;

	return((ioctl(0, FIONREAD, (char *) &x) < 0) ? 0 : x);
}

/*
 * panic - just exit, as quickly as we can.
 */
void
panic(s)
	char *s;
{

	(void) fputs("panic: ", stderr);
	(void) fputs(s, stderr);
	(void) fputc('\n', stderr);
	abort();		/* To leave a core image. */
}

#ifndef NO_DPROMPT
/*
 * A program to return TRUE if we wait for 2 seconds without anything
 * happening, else return FALSE.  Cribbed from mod.sources xmodem.
 */
int
ttwait()
{
	fd_set		readfds;
	struct timeval	tmout;

	FD_ZERO(&readfds);   
	FD_SET(0, &readfds);

	tmout.tv_sec = 2;
	tmout.tv_usec = 0;

	if ((select(1, &readfds, NULL, NULL, &tmout)) == 0)
		return (TRUE);
	return (FALSE);
}
#endif /* NO_DPROMPT */
