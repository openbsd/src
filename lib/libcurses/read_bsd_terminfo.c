/*	$OpenBSD: read_bsd_terminfo.c,v 1.6 1998/11/19 18:08:54 millert Exp $	*/

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
static char rcsid[] = "$OpenBSD: read_bsd_terminfo.c,v 1.6 1998/11/19 18:08:54 millert Exp $";
#endif

#include <curses.priv.h>
#include <tic.h>
#include <term.h>	/* lines, columns, cur_term */
#include <term_entry.h>

#define PVECSIZ 32
#define	_PATH_TERMINFO	"/usr/share/misc/terminfo"

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
    int    i;
    char   envterm[PATH_MAX];		/* local copy of $TERMINFO */
    char   hometerm[PATH_MAX];		/* local copy of $HOME/.terminfo */
    char  *pathvec[PVECSIZ];		/* to point to names in pathbuf */
    char   namecpy[MAX_NAME_SIZE+1];
    long   num;
    size_t len;

    fname = pathvec;
    /* $TERMINFO may hold a path to a terminfo file */
    if (!issetugid() && (p = getenv("TERMINFO")) != NULL) {
	len = strlcpy(envterm, p, sizeof(envterm));
	if (len < sizeof(envterm))
	    *fname++ = envterm;
    }

    /* Also check $HOME/.terminfo if it exists */
    if (!issetugid() && (p = getenv("HOME")) != NULL) {
	len = snprintf(hometerm, sizeof(hometerm), "%s/.terminfo", p);
	if (len < sizeof(hometerm))
	    *fname++ = hometerm;
    }

    /* Finally we check the system terminfo file and mark the end of vector */
    *fname++ = _PATH_TERMINFO;
    *fname = (char *) 0;

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
