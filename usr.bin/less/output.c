/*
 * Copyright (C) 1984-2012  Mark Nudelman
 * Modified for use with illumos by Garrett D'Amore.
 * Copyright 2014 Garrett D'Amore <garrett@damore.org>
 *
 * You may distribute under the terms of either the GNU General Public
 * License or the Less License, as specified in the README file.
 *
 * For more information, see the README file.
 */

/*
 * High level routines dealing with the output to the screen.
 */

#include "less.h"

int errmsgs;	/* Count of messages displayed by error() */

extern volatile sig_atomic_t sigs;
extern int sc_width;
extern int so_s_width, so_e_width;
extern int screen_trashed;
extern int any_display;
extern int is_tty;
extern int oldbot;

static int need_clr;

/*
 * Display the line which is in the line buffer.
 */
void
put_line(void)
{
	int c;
	int i;
	int a;

	if (ABORT_SIGS()) {
		/*
		 * Don't output if a signal is pending.
		 */
		screen_trashed = 1;
		return;
	}

	for (i = 0;  (c = gline(i, &a)) != '\0';  i++) {
		at_switch(a);
		if (c == '\b')
			putbs();
		else
			(void) putchr(c);
	}

	at_exit();
}

static char obuf[OUTBUF_SIZE];
static char *ob = obuf;

/*
 * Flush buffered output.
 *
 * If we haven't displayed any file data yet,
 * output messages on error output (file descriptor 2),
 * otherwise output on standard output (file descriptor 1).
 *
 * This has the desirable effect of producing all
 * error messages on error output if standard output
 * is directed to a file.  It also does the same if
 * we never produce any real output; for example, if
 * the input file(s) cannot be opened.  If we do
 * eventually produce output, code in edit() makes
 * sure these messages can be seen before they are
 * overwritten or scrolled away.
 */
void
flush(int ignore_errors)
{
	int n;
	int fd;
	ssize_t nwritten;

	n = (intptr_t)ob - (intptr_t)obuf;
	if (n == 0)
		return;

	fd = (any_display) ? STDOUT_FILENO : STDERR_FILENO;
	nwritten = write(fd, obuf, n);
	if (nwritten != n) {
		if (nwritten == -1 && !ignore_errors)
			quit(QUIT_ERROR);
		screen_trashed = 1;
	}
	ob = obuf;
}

/*
 * Output a character.
 */
int
putchr(int c)
{
	if (need_clr) {
		need_clr = 0;
		clear_bot();
	}
	/*
	 * Some versions of flush() write to *ob, so we must flush
	 * when we are still one char from the end of obuf.
	 */
	if (ob >= &obuf[sizeof (obuf)-1])
		flush(0);
	*ob++ = (char)c;
	return (c);
}

/*
 * Output a string.
 */
void
putstr(const char *s)
{
	while (*s != '\0')
		(void) putchr(*s++);
}


/*
 * Convert an integral type to a string.
 */
#define	TYPE_TO_A_FUNC(funcname, type)		\
void						\
funcname(type num, char *buf, size_t len)	\
{						\
	int neg = (num < 0);			\
	char tbuf[23];	\
	char *s = tbuf + sizeof (tbuf);		\
	if (neg)				\
		num = -num;			\
	*--s = '\0';				\
	do {					\
		*--s = (num % 10) + '0';	\
	} while ((num /= 10) != 0);		\
	if (neg)				\
		 *--s = '-';			\
	(void) strlcpy(buf, s, len);		\
}

TYPE_TO_A_FUNC(postoa, off_t)
TYPE_TO_A_FUNC(inttoa, int)

/*
 * Output an integer in a given radix.
 */
static int
iprint_int(int num)
{
	char buf[11];

	inttoa(num, buf, sizeof (buf));
	putstr(buf);
	return (strlen(buf));
}

/*
 * Output a line number in a given radix.
 */
static int
iprint_linenum(off_t num)
{
	char buf[21];

	postoa(num, buf, sizeof(buf));
	putstr(buf);
	return (strlen(buf));
}

/*
 * This function implements printf-like functionality
 * using a more portable argument list mechanism than printf's.
 */
static int
less_printf(const char *fmt, PARG *parg)
{
	char *s;
	int col;

	col = 0;
	while (*fmt != '\0') {
		if (*fmt != '%') {
			(void) putchr(*fmt++);
			col++;
		} else {
			++fmt;
			switch (*fmt++) {
			case 's':
				s = parg->p_string;
				parg++;
				while (*s != '\0') {
					(void) putchr(*s++);
					col++;
				}
				break;
			case 'd':
				col += iprint_int(parg->p_int);
				parg++;
				break;
			case 'n':
				col += iprint_linenum(parg->p_linenum);
				parg++;
				break;
			}
		}
	}
	return (col);
}

/*
 * Get a RETURN.
 * If some other non-trivial char is pressed, unget it, so it will
 * become the next command.
 */
void
get_return(void)
{
	int c;

	c = getchr();
	if (c != '\n' && c != '\r' && c != ' ' && c != READ_INTR)
		ungetcc(c);
}

/*
 * Output a message in the lower left corner of the screen
 * and wait for carriage return.
 */
void
error(const char *fmt, PARG *parg)
{
	int col = 0;
	static char return_to_continue[] = "  (press RETURN)";

	errmsgs++;

	if (any_display && is_tty) {
		if (!oldbot)
			squish_check();
		at_exit();
		clear_bot();
		at_enter(AT_STANDOUT);
		col += so_s_width;
	}

	col += less_printf(fmt, parg);

	if (!(any_display && is_tty)) {
		(void) putchr('\n');
		return;
	}

	putstr(return_to_continue);
	at_exit();
	col += sizeof (return_to_continue) + so_e_width;

	get_return();
	lower_left();
	clear_eol();

	if (col >= sc_width)
		/*
		 * Printing the message has probably scrolled the screen.
		 * {{ Unless the terminal doesn't have auto margins,
		 *    in which case we just hammered on the right margin. }}
		 */
		screen_trashed = 1;

	flush(0);
}

static char intr_to_abort[] = "... (interrupt to abort)";

/*
 * Output a message in the lower left corner of the screen
 * and don't wait for carriage return.
 * Usually used to warn that we are beginning a potentially
 * time-consuming operation.
 */
void
ierror(const char *fmt, PARG *parg)
{
	at_exit();
	clear_bot();
	at_enter(AT_STANDOUT);
	(void) less_printf(fmt, parg);
	putstr(intr_to_abort);
	at_exit();
	flush(0);
	need_clr = 1;
}

/*
 * Output a message in the lower left corner of the screen
 * and return a single-character response.
 */
int
query(const char *fmt, PARG *parg)
{
	int c;
	int col = 0;

	if (any_display && is_tty)
		clear_bot();

	(void) less_printf(fmt, parg);
	c = getchr();

	if (!(any_display && is_tty)) {
		(void) putchr('\n');
		return (c);
	}

	lower_left();
	if (col >= sc_width)
		screen_trashed = 1;
	flush(0);

	return (c);
}
