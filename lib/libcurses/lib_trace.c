
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

/*
 *	lib_trace.c - Tracing/Debugging routines
 */

#ifndef TRACE
#define TRACE			/* turn on internal defs for this module */
#endif

#include <curses.priv.h>

MODULE_ID("Id: lib_trace.c,v 1.23 1997/05/02 00:13:07 tom Exp $")

#include <ctype.h>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

unsigned _nc_tracing = 0;
const char *_nc_tputs_trace = "";
long _nc_outchars;
int _nc_optimize_enable = OPTIMIZE_ALL;

static FILE *	tracefp;	/* default to writing to stderr */

void trace(const unsigned int tracelevel)
{
static bool	been_here = FALSE;

   	_nc_tracing = tracelevel;
	if (! been_here && tracelevel) {
		been_here = TRUE;

		if ((tracefp = fopen("trace", "w")) == 0) {
			perror("curses: Can't open 'trace' file: ");
			exit(EXIT_FAILURE);
		}
		/* Try to set line-buffered mode, or (failing that) unbuffered,
		 * so that the trace-output gets flushed automatically at the
		 * end of each line.  This is useful in case the program dies. 
		 */
#if HAVE_SETVBUF	/* ANSI */
		(void) setvbuf(tracefp, (char *)0, _IOLBF, 0);
#elif HAVE_SETBUF	/* POSIX */
		(void) setbuffer(tracefp, (char *)0);
#endif
		_tracef("TRACING NCURSES version %s (%d)",
			NCURSES_VERSION, NCURSES_VERSION_PATCH);
	}
}

const char *_nc_visbuf2(int bufnum, const char *buf)
/* visibilize a given string */
{
char *vbuf;
char *tp;
int c;

	if (buf == 0)
	    return("(null)");

	tp = vbuf = _nc_trace_buf(bufnum, (strlen(buf) * 4) + 5);
	*tp++ = '"';
    	while ((c = *buf++) != '\0') {
		if (c == '"') {
			*tp++ = '\\'; *tp++ = '"';
		} else if (is7bits(c) && (isgraph(c) || c == ' ')) {
			*tp++ = c;
		} else if (c == '\n') {
			*tp++ = '\\'; *tp++ = 'n';
		} else if (c == '\r') {
			*tp++ = '\\'; *tp++ = 'r';
		} else if (c == '\b') {
			*tp++ = '\\'; *tp++ = 'b';
		} else if (c == '\033') {
			*tp++ = '\\'; *tp++ = 'e';
		} else if (is7bits(c) && iscntrl(c)) {
			*tp++ = '\\'; *tp++ = '^'; *tp++ = '@' + c;
		} else {
			sprintf(tp, "\\%03o", c & 0xff);
			tp += strlen(tp);
		}
	}
	*tp++ = '"';
	*tp++ = '\0';
	return(vbuf);
}

const char *_nc_visbuf(const char *buf)
{
	return _nc_visbuf2(0, buf);
}

void
_tracef(const char *fmt, ...)
{
static const char Called[] = T_CALLED("");
static const char Return[] = T_RETURN("");
static int level;
va_list ap;
bool	before = FALSE;
bool	after = FALSE;
int	doit = _nc_tracing;

	if (strlen(fmt) >= sizeof(Called) - 1) {
		if (!strncmp(fmt, Called, sizeof(Called)-1)) {
			before = TRUE;
			level++;
		} else if (!strncmp(fmt, Return, sizeof(Return)-1)) {
			after = TRUE;
		}
		if (before || after) {
			if ((level <= 1)
			 || (doit & TRACE_ICALLS) != 0)
				doit &= (TRACE_CALLS|TRACE_CCALLS);
			else
				doit = 0;
		}
	}

	if (doit != 0) {
		if (tracefp == 0)
			tracefp = stderr;
		if (before || after) {
			int n;
			for (n = 1; n < level; n++)
				fputs("+ ", tracefp);
		}
		va_start(ap, fmt);
		vfprintf(tracefp, fmt, ap);
		fputc('\n', tracefp);
		va_end(ap);
		fflush(tracefp);
	}

	if (after && level)
		level--;
}

/* Trace 'int' return-values */
int _nc_retrace_int(int code)
{
	T((T_RETURN("%d"), code));
	return code;
}

/* Trace 'char*' return-values */
char * _nc_retrace_ptr(char * code)
{
	T((T_RETURN("%s"), _nc_visbuf(code)));
	return code;
}

/* Trace 'WINDOW *' return-values */
WINDOW *_nc_retrace_win(WINDOW *code)
{
	T((T_RETURN("%p"), code));
	return code;
}
