/*	$OpenBSD: getterm.c,v 1.16 1998/01/17 16:35:06 millert Exp $	*/

/*
 * Copyright (c) 1996 SigmaSoft, Th. Lockert <tholo@sigmasoft.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by SigmaSoft, Th.  Lockert.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: getterm.c,v 1.16 1998/01/17 16:35:06 millert Exp $";
#endif

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#include "term.h"
#include "term.private.h"
#include "pathnames.h"

#define	PVECSIZ	32
#define	MAXSIZE	256	/* Maximum allowed size of a terminal name field */

TERMINAL *cur_term;
char *_ti_buf;
char *UP;
char *BC;
char PC;
int LINES, COLS, TABSIZE;
char ttytype[MAXSIZE];

static int _ti_use_env = TRUE;

/*
 * Internal routine to read in a terminal description.
 * Currently supports reading from termcap files and databases
 * only; should be extended to also support reading from binary
 * terminfo files
 *
 * Will also set up global variables for compatibility with old
 * termcap routines, as well as populate cur_term's capability
 * variables.  If called from the termcap tgetent() compatibility
 * routine, it will copy the termcap entry into the buffer
 * provided to that routine.  Note that no other code in this
 * library depends on the termcap entry being kept
 */
int
_ti_gettermcap(name)
     const char *name;
{
    register char *p;
    register char *cp;
    char  *dummy;
    char **fname;
    char  *home;
    int    i;
    char   pathbuf[MAXPATHLEN+1];	/* holds raw path of filenames */
    char  *pathvec[PVECSIZ];		/* to point to names in pathbuf */
    char  *termpath;
    long   num;

    fname = pathvec;
    p = pathbuf;
    cp = getenv("TERMCAP");
    /*
     * TERMCAP can have one of two things in it. It can be the
     * name of a file to use instead of /etc/termcap. In this
     * case it better start with a "/". Or it can be an entry to
     * use so we don't have to read the file. In this case it
     * has to already have the newlines crunched out.  If TERMCAP
     * does not hold a file name then a path of names is searched
     * instead.  The path is found in the TERMPATH variable, or
     * becomes "$HOME/.termcap /etc/termcap" if no TERMPATH exists.
     */
    if (!cp || *cp != '/') {	/* no TERMCAP or it holds an entry */
	if ((termpath = getenv("TERMPATH")) != NULL)
	    strncpy(pathbuf, termpath, sizeof(pathbuf)-1);
	else {
	    if ((home = getenv("HOME")) != NULL) {
		/* set up default */
		/* $HOME first */
		strncpy(pathbuf, home, sizeof(pathbuf) - 1 -
		    strlen(_PATH_CAPDEF) - 1);
		pathbuf[sizeof(pathbuf) - 1 - strlen(_PATH_CAPDEF) - 1] = '\0';
		p += strlen(pathbuf);	/* path, looking in */
		*p++ = '/';
	    }	/* if no $HOME look in current directory */
	    strncpy(p, _PATH_CAPDEF, sizeof(pathbuf) - 1 - (p - pathbuf));
	}
    }
    else					/* user-defined name in TERMCAP */
	strncpy(pathbuf, cp, sizeof(pathbuf)-1);	/* still can be tokenized */
    pathbuf[sizeof(pathbuf)-1] = '\0';

    *fname++ = pathbuf;	/* tokenize path into vector of names */
    while (*++p)
	if (*p == ' ' || *p == ':') {
	    *p = '\0';
	    while (*++p)
		if (*p != ' ' && *p != ':')
		    break;
	    if (*p == '\0')
		break;
	    *fname++ = p;
	    if (fname >= pathvec + PVECSIZ) {
		fname--;
		break;
	    }
	}
    *fname = (char *) 0;			/* mark end of vector */
    if (cp && *cp && *cp != '/')
	if (cgetset(cp) < 0)
	    return (-2);

    dummy = NULL;
    i = cgetent(&dummy, pathvec, (char *)name);      
	
    if (i == 0) {
	char *s;

	if ((s = home = strchr(dummy, ':')) == NULL) {
	    cur_term->name = cur_term->names = strdup(name);
	    strncpy(ttytype, name, MAXSIZE - 1);
	    ttytype[MAXSIZE - 1] = '\0';
	}
	else {
	    size_t n;

	    n = s - dummy - (dummy[2] == '|' ? 3 : 0);
	    cur_term->names = malloc(n + 1);
	    strncpy(cur_term->names, dummy + (dummy[2] == '|' ? 3 : 0), n);
	    cur_term->names[n] = '\0';
	    strncpy(ttytype, dummy + (dummy[2] == '|' ? 3 : 0),
		    (size_t)MIN(MAXSIZE - 1, s - dummy));
	    ttytype[MAXSIZE - 1] = '\0';
	    *home = '\0';
	    while (s > dummy && *s != '|')
		s--;
	    if (s > dummy)
		s++;
	    cur_term->name = strdup(s);
	    *home = ':';
	}
	for (i = 0; i < _ti_numcaps; i++) {
	    switch (_ti_captoidx[i].type) {
		case TYPE_BOOL:
		    if (cgetcap(dummy, (char *)_ti_captoidx[i].name, ':') == NULL)
			cur_term->bools[_ti_captoidx[i].idx] = 0;
		    else
			cur_term->bools[_ti_captoidx[i].idx] = 1;
		    break;
		case TYPE_NUM:
		    if (cgetnum(dummy, (char *)_ti_captoidx[i].name, &num) < 0)
			cur_term->nums[_ti_captoidx[i].idx] = 0;
		    else
			cur_term->nums[_ti_captoidx[i].idx] = (int)num;
		    break;
		case TYPE_STR:
		    if (cgetstr(dummy, (char *)_ti_captoidx[i].name, &s) < 0)
			cur_term->strs[_ti_captoidx[i].idx] = NULL;
		    else {
			cur_term->strs[_ti_captoidx[i].idx] = _ti_captoinfo(s);
			free(s);
		    }
		    break;
	    }
	}
	if (_ti_buf) {
	    strncpy(_ti_buf, dummy, 1023);
	    _ti_buf[1023] = '\0';
	    if ((cp = strrchr(_ti_buf, ':')) != NULL)
		if (cp[1] != '\0')
		    cp[1] = '\0';
	}
	i = 0;
    }

    /* We are done with the returned termcap buffer now; free it */
    if (dummy)
	free(dummy);

    /* we found a "tc" reference loop, return error */
    if (i == -3)
	return (-1);

    return (i + 1);
}

/*
 * Internal routine to read in a terminal description.
 * Currently supports reading from termcap files and databases
 * only; should be extended to also support reading from binary
 * terminfo files
 *
 * Will also set up global variables for compatibility with old
 * termcap routines, as well as populate cur_term's capability
 * variables.  If called from the termcap tgetent() compatibility
 * routine, it will copy the termcap entry into the buffer
 * provided to that routine.  Note that no other code in this
 * library depends on the termcap entry being kept
 */
int
_ti_getterminfo(name)
     const char *name;
{
    register char *p;
    register char *cp;
    char  *dummy;
    char **fname;
    char  *home;
    int    i;
    char   pathbuf[MAXPATHLEN+1];	/* holds raw path of filenames */
    char  *pathvec[PVECSIZ];		/* to point to names in pathbuf */
    char  *termpath;
    long   num;

    fname = pathvec;
    p = pathbuf;
    /*
     * TERMCAP can have one of two things in it. It can be the
     * name of a file to use instead of /etc/termcap. In this
     * case it better start with a "/". Or it can be an entry to
     * use so we don't have to read the file. In this case it
     * has to already have the newlines crunched out.  If TERMCAP
     * does not hold a file name then a path of names is searched
     * instead.  The path is found in the TERMINFO variable, or
     * becomes "$HOME/.terminfo /usr/share/misc/terminfo" if no
     * TERMINFO exists.
     */
    if ((termpath = getenv("TERMINFO")) != NULL)
	strncpy(pathbuf, termpath, sizeof(pathbuf) - 1);
    else {
	if ((home = getenv("HOME")) != NULL) {
	    /* set up default */
	    /* $HOME first */
	    strncpy(pathbuf, home, sizeof(pathbuf) - 1 -
		strlen(_PATH_INFODEF) - 1);
	    pathbuf[sizeof(pathbuf) - 1 - strlen(_PATH_INFODEF) - 1] = '\0';
	    p += strlen(home);	/* path, looking in */
	    *p++ = '/';
	}	/* if no $HOME look in current directory */
	strncpy(p, _PATH_INFODEF, sizeof(pathbuf) - 1 - (p - pathbuf));
    }
    pathbuf[sizeof(pathbuf) - 1] = '\0';

    *fname++ = pathbuf;	/* tokenize path into vector of names */
    while (*++p)
	if (*p == ' ' || *p == ':') {
	    *p = '\0';
	    while (*++p)
		if (*p != ' ' && *p != ':')
		    break;
	    if (*p == '\0')
		break;
	    *fname++ = p;
	    if (fname >= pathvec + PVECSIZ) {
		fname--;
		break;
	    }
	}
    *fname = (char *) 0;			/* mark end of vector */
    (void) cgetset(NULL);

    dummy = NULL;
    i = cgetent(&dummy, pathvec, (char *)name);      
	
    if (i == 0) {
	char *s;

	if ((s = home = strchr(dummy, ':')) == NULL) {
	    cur_term->name = cur_term->names = strdup(name);
	    strncpy(ttytype, name, MAXSIZE - 1);
	    ttytype[MAXSIZE - 1] = '\0';
	}
	else {
	    size_t n;

	    n = s - dummy - (dummy[2] == '|' ? 3 : 0);
	    cur_term->names = malloc(n + 1);
	    strncpy(cur_term->names, dummy + (dummy[2] == '|' ? 3 : 0), n);
	    cur_term->names[n] = '\0';
	    strncpy(ttytype, dummy + (dummy[2] == '|' ? 3 : 0),
		    (size_t)MIN(MAXSIZE - 1, s - dummy));
	    ttytype[MAXSIZE - 1] = '\0';
	    *home = '\0';
	    while (s > dummy && *s != '|')
		s--;
	    if (s > dummy)
		s++;
	    cur_term->name = strdup(s);
	    *home = ':';
	}
	for (i = 0 ; i < _tBoolCnt ; i++) {
	    if (cgetcap(dummy, (char *)boolcodes[i], ':') == NULL)
		cur_term->bools[i] = 0;
	    else
		cur_term->bools[i] = 1;
	}
	for (i = 0 ; i < _tNumCnt ; i++) {
	    if (cgetnum(dummy, (char *)numcodes[i], &num) < 0)
		cur_term->nums[i] = 0;
	    else
		cur_term->nums[i] = (int)num;
	}
	for (i = 0 ; i < _tStrCnt ; i++) {
	    if (cgetstr(dummy, (char *)strcodes[i], &s) < 0)
		cur_term->strs[i] = NULL;
	    else
		cur_term->strs[i] = s;
	}
	if (_ti_buf) {
	    strncpy(_ti_buf, dummy, 1023);
	    _ti_buf[1023] = '\0';
	    if ((cp = strrchr(_ti_buf, ':')) != NULL)
		if (cp[1] != '\0')
		    cp[1] = '\0';
	}
	i = 0;
    }

    /* We are done with the returned termcap buffer now; free it */
    if (dummy)
	free(dummy);

    /* we found a "tc" reference loop, return error */
    if (i == -3)
	return (-1);

    return (i + 1);
}

void
_ti_get_screensize(linep, colp, tabp)
    int *linep;
    int *colp;
    int *tabp;
{
    char *s;
#ifdef TIOCGWINSZ
    struct winsize winsz;
#endif

    *linep = lines;
    *colp = columns;
    if (tabp != NULL) {
	if (init_tabs == 0)
	    init_tabs = *tabp = 8;
	else
	    *tabp = init_tabs;
    }
    if (_ti_use_env) {
#ifdef TIOCGWINSZ
	/*
	 * get the current window size, overrides entries in termcap
	 */
	if (ioctl(cur_term->fd, TIOCGWINSZ, &winsz) >= 0) {
	    if (winsz.ws_row > 0)
		*linep = winsz.ws_row;
	    if (winsz.ws_col > 0)
		*colp = winsz.ws_col;
	}
#endif
	/*
	 * LINES and COLS environment variables overrides any other
	 * method of getting the terminal window size
	 */
	if ((s = getenv("LINES")) != NULL)
	    *linep = atoi(s);
	if ((s = getenv("COLUMNS")) != NULL)
	    *colp = atoi(s);
    }
    lines = *linep;
    columns = *colp;
}

int
_ti_getterm(name)
     const char *name;
{
    int ret = 1;
    char *s;

    s = getenv("TERMCAP");
    if (s && *s == '/')
	s = NULL;
    if (_ti_buf || s) {
	if (_ti_gettermcap(name) != 1) {
	    del_curterm(cur_term);
	    if ((cur_term = calloc(sizeof(TERMINAL), 1)) == NULL)
		errx(1, "No memory for terminal description");
	    ret = _ti_getterminfo(name);
	}
    }
    else {
	if (_ti_getterminfo(name) != 1) {
	    del_curterm(cur_term);
	    if ((cur_term = calloc(sizeof(TERMINAL), 1)) == NULL)
		errx(1, "No memory for terminal description");
	    ret = _ti_gettermcap(name);
	}
    }

    if (ret == 1) {
	_ti_fillcap(cur_term);
	UP = cursor_up;
	BC = backspace_if_not_bs;
	PC = pad_char ? pad_char[0] : '\0';
	_ti_get_screensize(&LINES, &COLS, &TABSIZE);
    }
    return ret;
}

/*
 * Allows the calling program to not have the window size or
 * environment variables LINES and COLS override the termcap
 * or terminfo lines/columns specifications
 */
void
use_env(flag)
    int flag;
{
    _ti_use_env = flag;
}
