/*	$OpenBSD: read_bsd_terminfo.c,v 1.2 1998/07/24 06:10:42 downsj Exp $	*/

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
static char rcsid[] = "$OpenBSD: read_bsd_terminfo.c,v 1.2 1998/07/24 06:10:42 downsj Exp $";
#endif

#include <curses.priv.h>
#include <tic.h>
#include <term.h>	/* lines, columns, cur_term */
#include <term_entry.h>

#define PVECSIZ 32

/* XXX - belongs in a header file */
#define	_PATH_INFODEF	".terminfo /usr/share/misc/terminfo"

/*
 * Look up ``tn'' in the BSD terminfo.db file and fill in ``tp''
 * with the info we find there.
 * Returns 1 on success, 0 on failure.
 */
int
_nc_read_bsd_terminfo_entry(tn, tp)
     const char *tn;
     TERMTYPE *const tp;
{
    char  *p;
    char  *dummy;
    char **fname;
    char  *home;
    int    i;
    char   pathbuf[PATH_MAX];		/* holds raw path of filenames */
    char  *pathvec[PVECSIZ];		/* to point to names in pathbuf */
    char  *termpath;
    char   namecpy[MAX_NAME_SIZE+1];
    long   num;
    size_t len;

    fname = pathvec;
    p = pathbuf;
    /*
     * TERMINFO can have one of two things in it. It can be the name
     * of a file to use instead of /usr/share/misc/terminfo. In this
     * case it better start with a "/". Or it can be an entry to use
     * so we don't have to read the file. In this case it has to
     * already have the newlines crunched out.  If TERMINFO does not
     * hold a file name then a path of names is searched instead.
     * The path is found in the TERMINFO variable, or becomes
     * "$HOME/.terminfo /usr/share/misc/terminfo" if no TERMINFO
     * exists.
     */
    if ((termpath = getenv("TERMINFO")) != NULL)
	strlcpy(pathbuf, termpath, sizeof(pathbuf));
    else {
	/* $HOME/.terminfo or just .terminfo if no $HOME */
	if ((home = getenv("HOME")) != NULL) {
	    len = strlcpy(pathbuf, home,
		sizeof(pathbuf) - sizeof(_PATH_INFODEF));
	    if (len < sizeof(pathbuf) - sizeof(_PATH_INFODEF)) {
		p+= len;
		*p++ = '/';
	    }
	}
	strlcpy(p, _PATH_INFODEF, sizeof(pathbuf) - (p - pathbuf));
    }

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
    i = cgetent(&dummy, pathvec, (char *)tn);      
	
    if (i == 0) {
	_nc_init_entry(tp);

	/* Set terminal name(s) */
	if ((p = strchr(dummy, ':')) != NULL)
	    *p = '\0';
	if ((tp->str_table = tp->term_names = strdup(dummy)) == NULL)
	    return (0);
	_nc_set_type(_nc_first_name(tp->term_names));
	if (p)
	    *p = ':';

	/* Truncate overly-long names and aliases */
	(void)strlcpy(namecpy, tp->term_names, sizeof(namecpy));
	if ((p = strrchr(namecpy, '|')) != (char *)NULL)
	    *p = '\0';
	p = strtok(namecpy, "|");
	if (strlen(p) > MAX_ALIAS)
	    _nc_warning("primary name may be too long");
	while ((p = strtok((char *)NULL, "|")) != (char *)NULL)
	    if (strlen(p) > MAX_ALIAS)
		_nc_warning("alias `%s' may be too long", p);

	/* Copy capabilities */
	for (i = 0 ; i < BOOLCOUNT ; i++) {
	    if (cgetcap(dummy, (char *)boolnames[i], ':') == NULL)
		tp->Booleans[i] = FALSE;
	    else
		tp->Booleans[i] = TRUE;
	}
	for (i = 0 ; i < NUMCOUNT ; i++) {
	    if (cgetnum(dummy, (char *)numnames[i], &num) < 0)
		tp->Numbers[i] = 0;
	    else
		tp->Numbers[i] = (int)num;
	}
	for (i = 0 ; i < STRCOUNT ; i++) {
	    if (cgetstr(dummy, (char *)strnames[i], &p) < 0)
		tp->Strings[i] = NULL;
	    else
		tp->Strings[i] = p;
	}
	i = 0;
    }

    /* We are done with the returned getcap buffer now; free it */
    if (dummy)
	free(dummy);

    return ((i == 0));
}
