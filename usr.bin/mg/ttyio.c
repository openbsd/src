/*	$OpenBSD: ttyio.c,v 1.32 2008/02/05 12:53:38 reyk Exp $	*/

/* This file is in the public domain. */

/*
 * POSIX terminal I/O.
 *
 * The functions in this file negotiate with the operating system for
 * keyboard characters, and write characters to the display in a barely
 * buffered fashion.
 */
#include "def.h"

#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <termios.h>
#include <term.h>

#define NOBUF	512			/* Output buffer size. */

#ifndef TCSASOFT
#define TCSASOFT	0
#endif

int	ttstarted;
char	obuf[NOBUF];			/* Output buffer. */
size_t	nobuf;				/* Buffer count. */
struct	termios	oldtty;			/* POSIX tty settings. */
struct	termios	newtty;
int	nrow;				/* Terminal size, rows. */
int	ncol;				/* Terminal size, columns. */

/*
 * This function gets called once, to set up the terminal.
 * On systems w/o TCSASOFT we turn off off flow control,
 * which isn't really the right thing to do.
 */
void
ttopen(void)
{
	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
		panic("standard input and output must be a terminal");

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
ttraw(void)
{
	if (tcgetattr(0, &oldtty) < 0) {
		ewprintf("ttopen can't get terminal attributes");
		return (FALSE);
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
		return (FALSE);
	}
	ttstarted = 1;

	return (TRUE);
}

/*
 * This function gets called just before we go back home to the shell.
 * Put all of the terminal parameters back.
 * Under UN*X this just calls ttcooked(), but the ttclose() hook is in
 * because vttidy() in display.c expects it for portability reasons.
 */
void
ttclose(void)
{
	if (ttstarted) {
		if (ttcooked() == FALSE)
			panic("");	/* ttcooked() already printf'd */
		ttstarted = 0;
	}
}

/*
 * This function restores all terminal settings to their default values,
 * in anticipation of exiting or suspending the editor.
 */
int
ttcooked(void)
{
	ttflush();
	if (tcsetattr(0, TCSASOFT | TCSADRAIN, &oldtty) < 0) {
		ewprintf("ttclose can't tcsetattr");
		return (FALSE);
	}
	return (TRUE);
}

/*
 * Write character to the display.  Characters are buffered up,
 * to make things a little bit more efficient.
 */
int
ttputc(int c)
{
	if (nobuf >= NOBUF)
		ttflush();
	obuf[nobuf++] = c;
	return (c);
}

/*
 * Flush output.
 */
void
ttflush(void)
{
	ssize_t	 written;
	char	*buf = obuf;

	if (nobuf == 0)
		return;

	while ((written = write(fileno(stdout), buf, nobuf)) != nobuf) {
		if (written == -1) {
			if (errno == EINTR)
				continue;
			panic("ttflush write failed");
		}
		buf += written;
		nobuf -= written;
	}
	nobuf = 0;
}

/*
 * Read character from terminal. All 8 bits are returned, so that you
 * can use a multi-national terminal.
 */
int
ttgetc(void)
{
	char	c;
	ssize_t	ret;

	do {
		ret = read(STDIN_FILENO, &c, 1);
		if (ret == -1 && errno == EINTR) {
			if (winch_flag) {
				redraw(0, 0);
				winch_flag = 0;
			}
		} else if (ret == 1)
			break;
	} while (1);
	return ((int) c) & 0xFF;
}

/*
 * Returns TRUE if there are characters waiting to be read.
 */
int
charswaiting(void)
{
	int	x;

	return ((ioctl(0, FIONREAD, &x) < 0) ? 0 : x);
}

/*
 * panic - just exit, as quickly as we can.
 */
void
panic(char *s)
{
	ttclose();
	(void) fputs("panic: ", stderr);
	(void) fputs(s, stderr);
	(void) fputc('\n', stderr);
	exit(1);
}

/*
 * This function returns FALSE if any characters have showed up on the
 * tty before 'msec' milliseconds.
 */
int
ttwait(int msec)
{
	fd_set		readfds;
	struct timeval	tmout;

	FD_ZERO(&readfds);
	FD_SET(0, &readfds);

	tmout.tv_sec = msec/1000;
	tmout.tv_usec = msec - tmout.tv_sec * 1000;

	if ((select(1, &readfds, NULL, NULL, &tmout)) == 0)
		return (TRUE);
	return (FALSE);
}
