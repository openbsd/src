/*	$OpenBSD: read_bsd_terminfo.c,v 1.2 1999/01/22 04:50:43 millert Exp $	*/

/*
 * Copyright (c) 1998 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 4. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: read_bsd_terminfo.c,v 1.2 1999/01/22 04:50:43 millert Exp $";
#endif

#include <curses.priv.h>
#include <tic.h>
#include <term.h>	/* lines, columns, cur_term */
#include <term_entry.h>

#define PVECSIZ 3 * 2
#define	_PATH_TERMINFO	"/usr/share/misc/terminfo"

/*
 * Look up ``tn'' in the BSD terminfo.db file and fill in ``tp''
 * with the info we find there.
 * Returns 1 on success, 0 on failure.
 */
int
_nc_read_bsd_terminfo_entry(tn, filename, tp)
    const char *const tn;
    char *const filename;
    TERMTYPE *const tp;
{
    char  *p;
    char  *capbuf;
    char **fname;
    int    i, pathcnt;
    char   envterm[PATH_MAX];		/* local copy of $TERMINFO */
    char   hometerm[PATH_MAX];		/* local copy of $HOME/.terminfo */
    char  *pathvec[PVECSIZ];		/* list of possible terminfo files */
    char   namecpy[MAX_NAME_SIZE+1];
    long   num;
    size_t len;

    fname = pathvec;
    pathcnt = 1;
    /* $TERMINFO may hold a path to a terminfo file */
    if (!issetugid() && (p = getenv("TERMINFO")) != NULL) {
	len = strlcpy(envterm, p, sizeof(envterm));
	if (len < sizeof(envterm))
	    pathcnt++;
	    *fname++ = envterm;
	    *fname++ = NULL;
    }

    /* Also check $HOME/.terminfo if it exists */
    if (!issetugid() && (p = getenv("HOME")) != NULL) {
	len = snprintf(hometerm, sizeof(hometerm), "%s/.terminfo", p);
	if (len < sizeof(hometerm))
	    pathcnt++;
	    *fname++ = hometerm;
	    *fname++ = NULL;
    }

    /* Finally we check the system terminfo file */
    *fname++ = _PATH_TERMINFO;
    *fname = NULL;

    /* Don't prepent any hardcoded entries. */
    (void) cgetset(NULL);

    /*
     * We can't pass a normal vector in to cgetent(3) because
     * we need to know which of the paths in pathvec we actually
     * used (for the filename copyout parameter).
     * Therefore, we kludge things a bit...
     */
    for (fname = pathvec, i = 1; fname != pathvec + pathcnt * 2 && i != 0; ) {
	capbuf = NULL;
	i = cgetent(&capbuf, fname, (char *)tn);      
	    
	if (i == 0) {
	    /* Set copyout parameter and init term description */
	    (void)strlcpy(filename, *fname, PATH_MAX);
	    _nc_init_entry(tp);

	    /* Set terminal name(s) */
	    if ((p = strchr(capbuf, ':')) != NULL)
		*p = '\0';
	    if ((tp->str_table = tp->term_names = strdup(capbuf)) == NULL)
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
		if (cgetcap(capbuf, (char *)boolnames[i], ':') == NULL)
		    tp->Booleans[i] = FALSE;
		else
		    tp->Booleans[i] = TRUE;
	    }
	    for (i = 0 ; i < NUMCOUNT ; i++) {
		if (cgetnum(capbuf, (char *)numnames[i], &num) < 0)
		    tp->Numbers[i] = 0;
		else
		    tp->Numbers[i] = (int)num;
	    }
	    for (i = 0 ; i < STRCOUNT ; i++) {
		if (cgetstr(capbuf, (char *)strnames[i], &p) < 0)
		    tp->Strings[i] = NULL;
		else
		    tp->Strings[i] = p;
	    }
	    i = 0;
	}
	/* Increment by two since we have that NULL in there */
	fname += 2;

	/* We are done with the returned getcap buffer now; free it */
	cgetclose();
	if (capbuf)
	    free(capbuf);
    }

    return ((i == 0));
}
