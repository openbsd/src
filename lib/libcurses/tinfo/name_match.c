/*	$OpenBSD: name_match.c,v 1.1 1999/01/18 19:10:21 millert Exp $	*/

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

#include <curses.priv.h>
#include <term.h>
#include <tic.h>

MODULE_ID("$From: name_match.c,v 1.7 1998/09/19 20:27:49 Todd.Miller Exp $")

/*
 *	_nc_first_name(char *names)
 *
 *	Extract the primary name from a compiled entry.
 */

char *_nc_first_name(const char *const sp)
/* get the first name from the given name list */
{
    static char	buf[MAX_NAME_SIZE+1];
    register char *cp;

    (void) strlcpy(buf, sp, sizeof(buf));

    cp = strchr(buf, '|');
    if (cp)
	*cp = '\0';

    return(buf);
}

/*
 *	int _nc_name_match(namelist, name, delim)
 *
 *	Is the given name matched in namelist?
 */

int _nc_name_match(const char *const namelst, const char *const name, const char *const delim)
/* microtune this, it occurs in several critical loops */
{
char namecopy[MAX_ENTRY_SIZE];	/* this may get called on a TERMCAP value */
register char *cp;

	if (namelst == 0)
		return(FALSE);
	(void) strlcpy (namecopy, namelst, sizeof(namecopy));
	if ((cp = strtok(namecopy, delim)) != 0) {
		do {
			/* avoid strcmp() function-call cost if possible */
			if (cp[0] == name[0] && strcmp(cp, name) == 0)
			    return(TRUE);
		} while
		    ((cp = strtok((char *)0, delim)) != 0);
	}
	return(FALSE);
}
