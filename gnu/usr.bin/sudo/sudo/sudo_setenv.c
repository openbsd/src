/*	$OpenBSD: sudo_setenv.c,v 1.9 1999/03/29 20:29:07 millert Exp $	*/

/*
 *  CU sudo version 1.5.9
 *  Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
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
 *  Todd C. Miller <Todd.Miller@courtesan.com> Fri Jun  3 18:32:19 MDT 1994
 */

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

#ifndef STDC_HEADERS
#ifdef HAVE_PUTENV
extern int putenv	__P((const char *));
#endif /* HAVE_PUTENV */
#ifdef HAVE_SETENV
extern int setenv	__P((char *, char *, int));
#endif /* HAVE_SETENV */
#endif /* !STDC_HEADERS */

#ifndef lint
static const char rcsid[] = "$Sudo: sudo_setenv.c,v 1.35 1999/03/29 04:05:13 millert Exp $";
#endif /* lint */


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
