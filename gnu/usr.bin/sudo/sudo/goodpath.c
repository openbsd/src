/*	$OpenBSD: goodpath.c,v 1.9 1999/03/29 20:29:03 millert Exp $	*/

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
 *  This module contains sudo_goodpath(3)
 *
 *  sudo_goodpath(3) takes a path to check and returns its argument
 *  if the path is stat(2)'able, a regular file, and executable by
 *  root.  The string's size should be <= MAXPATHLEN.
 *
 *  Todd C. Miller <Todd.Miller@courtesan.com> Sat Mar 25 21:58:17 MST 1995
 */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "sudo.h"

#ifndef STDC_HEADERS
extern int stat		__P((const char *, struct stat *));
#endif /* !STDC_HEADERS */

#ifndef lint
static const char rcsid[] = "$Sudo: goodpath.c,v 1.31 1999/03/29 04:05:08 millert Exp $";
#endif /* lint */

/******************************************************************
 *
 *  sudo_goodpath()
 *
 *  this function takes a path and makes sure it describes a a file
 *  that is a normal file and executable by root.
 */

char * sudo_goodpath(path)
    const char * path;
{
    struct stat statbuf;		/* for stat(2) */
    int err;				/* if stat(2) got an error */

    /* check for brain damage */
    if (path == NULL || path[0] == '\0')
	return(NULL);

    /* we need to be root for the stat */
    set_perms(PERM_ROOT, 0);

    err = stat(path, &statbuf);

    /* discard root perms */
    set_perms(PERM_USER, 0);

    /* stat(3) failed */
    if (err)
	return(NULL);

    /* make sure path describes an executable regular file */
    if (S_ISREG(statbuf.st_mode) && (statbuf.st_mode & 0000111)) {
	return((char *)path);
    } else {
	/* file is not executable/regular */
	errno = EACCES;
	return(NULL);
    }
}
