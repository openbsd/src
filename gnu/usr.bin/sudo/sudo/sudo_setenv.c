/*	$OpenBSD: sudo_setenv.c,v 1.6 1998/09/15 02:42:45 millert Exp $	*/

/*
 *  CU sudo version 1.5.6
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *******************************************************************
 *
 *  This module contains sudo_setenv().
 *  sudo_setenv(3) adds a string of the form "var=val" to the environment.
 *
 *  Todd C. Miller (millert@colorado.edu) Fri Jun  3 18:32:19 MDT 1994
 */

#ifndef lint
static char rcsid[] = "$From: sudo_setenv.c,v 1.26 1998/04/06 03:35:47 millert Exp $";
#endif /* lint */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "sudo.h"
#include <options.h>

#ifndef STDC_HEADERS
#ifdef HAVE_PUTENV
extern int putenv	__P((const char *));
#endif /* HAVE_PUTENV */
#ifdef HAVE_SETENV
extern int setenv	__P((char *, char *, int));
#endif /* HAVE_SETENV */
#endif /* !STDC_HEADERS */


/**********************************************************************
 *
 * sudo_setenv()
 *
 *  sudo_setenv() adds a string of the form "var=val" to the environment.
 *  If it is unable to expand the current environent it returns -1,
 *  else it returns 0.
 */

int sudo_setenv(var, val)
    char *var;
    char *val;
{

#ifdef HAVE_SETENV
    return(setenv(var, val, 1));
#else
    char *envstring, *tmp;

    envstring = tmp = (char *) malloc(strlen(var) + strlen(val) + 2);
    if (envstring == NULL)
	return(-1);

    while ((*tmp++ = *var++))
	;

    *(tmp-1) = '=';

    while ((*tmp++ = *val++))
	;

    return(putenv(envstring));
#endif /* HAVE_SETENV */
}
