/*
 * Copyright (c) 1984,1985,1989,1994,1995  Mark Nudelman
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice in the documentation and/or other materials provided with 
 *    the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR 
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT 
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR 
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE 
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN 
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


/*
 * High level routines dealing with the output to the screen.
 */

#include "less.h"

public int errmsgs;	/* Count of messages displayed by error() */
public int need_clr;

extern int sigs;
extern int sc_width;
extern int so_s_width, so_e_width;
extern int screen_trashed;
extern int any_display;

/*
 * Display the line which is in the line buffer.
 */
	public void
put_line()
{
	register int c;
	register int i;
	int a;
	int curr_attr;

	if (ABORT_SIGS())
	{
		/*
		 * Don't output if a signal is pending.
		 */
		screen_trashed = 1;
		return;
	}

	curr_attr = AT_NORMAL;

	for (i = 0;  (c = gline(i, &a)) != '\0';  i++)
	{
		if (a != curr_attr)
		{
			/*
			 * Changing attributes.
			 * Display the exit sequence for the old attribute
			 * and the enter sequence for the new one.
			 */
			switch (curr_attr)
			{
			case AT_UNDERLINE:	ul_exit();	break;
			case AT_BOLD:		bo_exit();	break;
			case AT_BLINK:		bl_exit();	break;
			case AT_STANDOUT:	so_exit();	break;
			}
			switch (a)
			{
			case AT_UNDERLINE:	ul_enter();	break;
			case AT_BOLD:		bo_enter();	break;
			case AT_BLINK:		bl_enter();	break;
			case AT_STANDOUT:	so_enter();	break;
			}
			curr_attr = a;
		}
		if (curr_attr == AT_INVIS)
			continue;
		if (c == '\b')
			putbs();
		else
			putchr(c);
	}

	switch (curr_attr)
	{
	case AT_UNDERLINE:	ul_exit();	break;
	case AT_BOLD:		bo_exit();	break;
	case AT_BLINK:		bl_exit();	break;
	case AT_STANDOUT:	so_exit();	break;
	}
}

static char obuf[1024];
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
	public void
flush()
{
	register int n;
	register int fd;

#if MSOFTC
	*ob = '\0';
	_outtext(obuf);
	ob = obuf;
#else
	n = ob - obuf;
	if (n == 0)
		return;
	fd = (any_display) ? 1 : 2;
	if (write(fd, obuf, n) != n)
		screen_trashed = 1;
	ob = obuf;
#endif
}

/*
 * Output a character.
 */
	public int
putchr(c)
	int c;
{
	if (ob >= &obuf[sizeof(obuf)])
		flush();
	if (need_clr)
	{
		need_clr = 0;
		clear_bot();
	}
#if MSOFTC
	if (c == '\n')
		putchr('\r');
#endif
	*ob++ = c;
	return (c);
}

/*
 * Output a string.
 */
	public void
putstr(s)
	register char *s;
{
	while (*s != '\0')
		putchr(*s++);
}


/*
 * Output an integer in a given radix.
 */
	static int
iprintnum(num, radix)
	int num;
	int radix;
{
	register char *s;
	int r;
	int neg;
	char buf[10];

	if (neg = (num < 0))
		num = -num;

	s = buf;
	do
	{
		*s++ = (num % radix) + '0';
	} while ((num /= radix) != 0);

	if (neg)
		*s++ = '-';
	r = s - buf;

	while (s > buf)
		putchr(*--s);
	return (r);
}

/*
 * This function implements printf-like functionality
 * using a more portable argument list mechanism than printf's.
 */
	static int
iprintf(fmt, parg)
	register char *fmt;
	PARG *parg;
{
	register char *s;
	register int n;
	register int col;

	col = 0;
	while (*fmt != '\0')
	{
		if (*fmt != '%')
		{
			putchr(*fmt++);
			col++;
		} else
		{
			++fmt;
			switch (*fmt++) {
			case 's':
				s = parg->p_string;
				parg++;
				while (*s != '\0')
				{
					putchr(*s++);
					col++;
				}
				break;
			case 'd':
				n = parg->p_int;
				parg++;
				col += iprintnum(n, 10);
				break;
			}
		}
	}
	return (col);
}

/*
 * Output a message in the lower left corner of the screen
 * and wait for carriage return.
 */
	public void
error(fmt, parg)
	char *fmt;
	PARG *parg;
{
	int c;
	int col = 0;
	static char return_to_continue[] = "  (press RETURN)";

	errmsgs++;

	if (any_display)
	{
		clear_bot();
		so_enter();
		col += so_s_width;
	}

	col += iprintf(fmt, parg);

	if (!any_display)
	{
		putchr('\n');
		return;
	}

	putstr(return_to_continue);
	so_exit();
	col += sizeof(return_to_continue) + so_e_width;

#if ONLY_RETURN
	while ((c = getchr()) != '\n' && c != '\r')
		bell();
#else
	c = getchr();
	if (c != '\n' && c != '\r' && c != ' ' && c != READ_INTR)
		ungetcc(c);
#endif
	lower_left();

	if (col >= sc_width)
		/*
		 * Printing the message has probably scrolled the screen.
		 * {{ Unless the terminal doesn't have auto margins,
		 *    in which case we just hammered on the right margin. }}
		 */
		screen_trashed = 1;

	flush();
}

static char intr_to_abort[] = "... (interrupt to abort)";

/*
 * Output a message in the lower left corner of the screen
 * and don't wait for carriage return.
 * Usually used to warn that we are beginning a potentially
 * time-consuming operation.
 */
	public void
ierror(fmt, parg)
	char *fmt;
	PARG *parg;
{
	clear_bot();
	so_enter();
	(void) iprintf(fmt, parg);
	putstr(intr_to_abort);
	so_exit();
	flush();
	need_clr = 1;
}

/*
 * Output a message in the lower left corner of the screen
 * and return a single-character response.
 */
	public int
query(fmt, parg)
	char *fmt;
	PARG *parg;
{
	register int c;
	int col = 0;

	if (any_display)
		clear_bot();

	(void) iprintf(fmt, parg);
	c = getchr();

	if (!any_display)
	{
		putchr('\n');
		return (c);
	}

	lower_left();
	if (col >= sc_width)
		screen_trashed = 1;
	flush();

	return (c);
}
