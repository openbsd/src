/*	$OpenBSD: lib_setup.c,v 1.1 1998/07/23 21:19:24 millert Exp $	*/

/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/


/*
 * Terminal setup routines common to termcap and terminfo:
 *
 *		use_env(bool)
 *		setupterm(char *, int, int *)
 */

#include <curses.priv.h>
#include <tic.h>	/* for MAX_NAME_SIZE */

#if defined(SVR4_TERMIO) && !defined(_POSIX_SOURCE)
#define _POSIX_SOURCE
#endif

#include <term.h>	/* lines, columns, cur_term */

MODULE_ID("$From: lib_setup.c,v 1.38 1998/04/04 19:08:22 juergen Exp $")

/****************************************************************************
 *
 * Terminal size computation
 *
 ****************************************************************************/

#if HAVE_SIZECHANGE
# if !defined(sun) || !HAVE_TERMIOS_H
#  if HAVE_SYS_IOCTL_H
#   include <sys/ioctl.h>
#  endif
# endif
#endif

#if NEED_PTEM_H
 /* On SCO, they neglected to define struct winsize in termios.h -- it's only
  * in termio.h and ptem.h (the former conflicts with other definitions).
  */
# include <sys/stream.h>
# include <sys/ptem.h>
#endif

/*
 * SCO defines TIOCGSIZE and the corresponding struct.  Other systems (SunOS,
 * Solaris, IRIX) define TIOCGWINSZ and struct winsize.
 */
#ifdef TIOCGSIZE
# define IOCTL_WINSIZE TIOCGSIZE
# define STRUCT_WINSIZE struct ttysize
# define WINSIZE_ROWS(n) (int)n.ts_lines
# define WINSIZE_COLS(n) (int)n.ts_cols
#else
# ifdef TIOCGWINSZ
#  define IOCTL_WINSIZE TIOCGWINSZ
#  define STRUCT_WINSIZE struct winsize
#  define WINSIZE_ROWS(n) (int)n.ws_row
#  define WINSIZE_COLS(n) (int)n.ws_col
# endif
#endif

extern TERMINAL *cur_term;

static int _use_env = TRUE;

static void do_prototype(void);

void use_env(bool f)
{
	_use_env = f;
}

int LINES, COLS, TABSIZE;

static void _nc_get_screensize(int *linep, int *colp)
/* Obtain lines/columns values from the environment and/or terminfo entry */
{
char	*rows, *cols, *p;
long	l;

	/* figure out the size of the screen */
	T(("screen size: terminfo lines = %d columns = %d", lines, columns));

	if (!_use_env)
	{
	    *linep = (int)lines;
	    *colp  = (int)columns;
	}
	else	/* usually want to query LINES and COLUMNS from environment */
	{
	    *linep = *colp = 0;

	    /* first, look for environment variables */
	    if ((rows = getenv("LINES")) != 0) {
		l = strtol(rows, &p, 10);
		if (p != rows && *p == '\0' && l != LONG_MIN && l <= INT_MAX)
		    *linep = (int)l;
	    }
	    if ((cols = getenv("COLUMNS")) != 0) {
		l = strtol(cols, &p, 10);
		if (p != cols && *p == '\0' && l != LONG_MIN && l <= INT_MAX)
		    *colp = (int)l;
	    }
	    T(("screen size: environment LINES = %d COLUMNS = %d",*linep,*colp));

#if HAVE_SIZECHANGE
	    /* if that didn't work, maybe we can try asking the OS */
	    if (*linep <= 0 || *colp <= 0)
	    {
		if (isatty(cur_term->Filedes))
		{
		    STRUCT_WINSIZE size;

		    errno = 0;
		    do {
			if (ioctl(cur_term->Filedes, IOCTL_WINSIZE, &size) < 0
				&& errno != EINTR)
			    goto failure;
		    } while
			(errno == EINTR);

		    *linep = WINSIZE_ROWS(size);
		    *colp  = WINSIZE_COLS(size);
		}
		/* FALLTHRU */
	    failure:;
	    }
#endif /* HAVE_SIZECHANGE */

	    /* if we can't get dynamic info about the size, use static */
	    if (*linep <= 0 || *colp <= 0)
		if (lines > 0 && columns > 0)
		{
		    *linep = (int)lines;
		    *colp  = (int)columns;
		}

	    /* the ultimate fallback, assume fixed 24x80 size */
	    if (*linep <= 0 || *colp <= 0)
	    {
		*linep = 24;
		*colp  = 80;
	    }

	    /*
	     * Put the derived values back in the screen-size caps, so
	     * tigetnum() and tgetnum() will do the right thing.
	     */
	    lines   = (short)(*linep);
	    columns = (short)(*colp);
	}

	T(("screen size is %dx%d", *linep, *colp));

#ifdef init_tabs
	if (init_tabs != -1)
		TABSIZE = (int)init_tabs;
	else
#endif /* init_tabs */
		TABSIZE = 8;
	T(("TABSIZE = %d", TABSIZE));

}

#if USE_SIZECHANGE
void _nc_update_screensize(void)
{
	int my_lines, my_cols;

	_nc_get_screensize(&my_lines, &my_cols);
	if (SP != 0 && SP->_resize != 0)
		SP->_resize(my_lines, my_cols);
}
#endif

/****************************************************************************
 *
 * Terminal setup
 *
 ****************************************************************************/

#define ret_error(code, fmt, arg)	if (errret) {\
					    *errret = code;\
					    returnCode(ERR);\
					} else {\
					    fprintf(stderr, fmt, arg);\
					    exit(EXIT_FAILURE);\
					}

#define ret_error0(code, msg)		if (errret) {\
					    *errret = code;\
					    returnCode(ERR);\
					} else {\
					    fprintf(stderr, msg);\
					    exit(EXIT_FAILURE);\
					}

#if USE_DATABASE
static int grab_entry(const char *const tn, TERMTYPE *const tp)
/* return 1 if entry found, 0 if not found, -1 if database not accessible */
{
	char	filename[PATH_MAX];
	int	status;
	int	_nc_read_bsd_terminfo_entry(const char *, TERMTYPE *); /* XXX */

#ifdef __OpenBSD__
	if ((status = _nc_read_bsd_terminfo_entry(tn, tp)) == 1)
	    return(1);
#endif /* __OpenBSD__ */

	if ((status = _nc_read_entry(tn, filename, tp)) == 1)
	    return(1);

#ifndef PURE_TERMINFO
	/*
	 * Try falling back on the termcap file.  Note: allowing this call
	 * links the entire terminfo/termcap compiler into the startup code.
	 * It's preferable to build a real terminfo database and use that.
	 */
	status = _nc_read_termcap_entry(tn, tp);
#endif /* PURE_TERMINFO */

	return(status);
}
#endif

char ttytype[NAMESIZE];

/*
 *	setupterm(termname, Filedes, errret)
 *
 *	Find and read the appropriate object file for the terminal
 *	Make cur_term point to the structure.
 *
 */

int setupterm(const char *tname, int Filedes, int *errret)
{
struct term	*term_ptr;
int status;

	T((T_CALLED("setupterm(\"%s\",%d,%p)"), tname, Filedes, errret));

	if (tname == 0) {
		tname = getenv("TERM");
		if (tname == 0 || *tname == '\0') {
			ret_error0(-1, "TERM environment variable not set.\n");
                }
	}
	if (strlen(tname) > MAX_NAME_SIZE) {
		ret_error(-1, "TERM environment must be <= %d characters.\n",
		    MAX_NAME_SIZE);
	}

	T(("your terminal name is %s", tname));

	term_ptr = typeCalloc(TERMINAL, 1);

	if (term_ptr == 0) {
		ret_error0(-1, "Not enough memory to create terminal structure.\n") ;
        }
#if USE_DATABASE
	status = grab_entry(tname, &term_ptr->type);
#else
	status = 0;
#endif

	/* try fallback list if entry on disk */
	if (status != 1)
	{
	    const TERMTYPE	*fallback = _nc_fallback(tname);

	    if (fallback)
	    {
		memcpy(&term_ptr->type, fallback, sizeof(TERMTYPE));
		status = 1;
	    }
	}

	if (status == -1)
	{
		ret_error0(-1, "terminals database is inaccessible\n");
	}
	else if (status == 0)
	{
		ret_error(0, "'%s': unknown terminal type.\n", tname);
	}

	set_curterm(term_ptr);

	if (command_character  &&  getenv("CC"))
		do_prototype();

	strlcpy(ttytype, cur_term->type.term_names, NAMESIZE);

	/*
	 * Allow output redirection.  This is what SVr3 does.
	 * If stdout is directed to a file, screen updates go
	 * to standard error.
	 */
	if (Filedes == STDOUT_FILENO && !isatty(Filedes))
	    Filedes = STDERR_FILENO;
	cur_term->Filedes = Filedes;

	_nc_get_screensize(&LINES, &COLS);

	if (errret)
		*errret = 1;

	T((T_CREATE("screen %s %dx%d"), tname, LINES, COLS));

	if (generic_type) {
		ret_error(0, "'%s': I need something more specific.\n", tname);
	}
	if (hard_copy) {
		ret_error(1, "'%s': I can't handle hardcopy terminals.\n", tname);
	}
	returnCode(OK);
}

/*
**	do_prototype()
**
**	Take the real command character out of the CC environment variable
**	and substitute it in for the prototype given in 'command_character'.
**
*/

static void
do_prototype(void)
{
int	i, j;
char	CC;
char	proto;
char    *tmp;

	tmp = getenv("CC");
	CC = *tmp;
	proto = *command_character;

	for (i=0; i < STRCOUNT; i++) {
		j = 0;
		while (cur_term->type.Strings[i][j]) {
			if (cur_term->type.Strings[i][j] == proto)
				cur_term->type.Strings[i][j] = CC;
			j++;
		}
	}
}
