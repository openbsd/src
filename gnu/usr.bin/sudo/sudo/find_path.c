/*	$OpenBSD: find_path.c,v 1.7 1998/11/13 22:44:34 millert Exp $	*/

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
 *  This module contains the find_path() function that returns
 *  TRUE if the command was found and FALSE if not.
 *  If find_path() returns TRUE, the copyin paramters command and
 *  ocommand contain the resolved and unresolved pathnames respectively.
 *  NOTE: if "." or "" exists in PATH it will be searched last.
 *
 *  Todd C. Miller (millert@colorado.edu) Sat Mar 25 21:50:36 MST 1995
 */

#ifndef lint
static char rcsid[] = "$From: find_path.c,v 1.74 1998/04/06 03:35:34 millert Exp $";
#endif /* lint */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "sudo.h"
#include <options.h>

#ifndef STDC_HEADERS
#ifndef __GNUC__		/* gcc has its own malloc */
extern char *malloc	__P((size_t));
#endif /* __GNUC__ */
extern char *getenv	__P((const char *));
extern char *strcpy	__P((char *, const char *));
extern int fprintf	__P((FILE *, const char *, ...));
extern ssize_t readlink	__P((const char *, VOID *, size_t));
extern int stat		__P((const char *, struct stat *));
extern int lstat	__P((const char *, struct stat *));
#ifdef HAVE_STRDUP
extern char *strdup	__P((const char *));
#endif /* HAVE_STRDUP */
#endif /* !STDC_HEADERS */


#ifndef _S_IFMT
#define _S_IFMT		S_IFMT
#endif /* _S_IFMT */
#ifndef _S_IFLNK
#define _S_IFLNK	S_IFLNK
#endif /* _S_IFLNK */


/*******************************************************************
 *
 *  find_path()
 *
 *  this function finds the full pathname for a command and
 *  stores it in a statically allocated array, returning a pointer
 *  to the array.
 */

char * find_path(file)
    char *file;			/* file to find */
{
    static char command[MAXPATHLEN]; /* qualified filename */
    register char *n;		/* for traversing path */
    char *path = NULL;		/* contents of PATH env var */
    char *origpath;		/* so we can free path later */
    char *result = NULL;	/* result of path/file lookup */
#ifndef IGNORE_DOT_PATH
    int checkdot = 0;		/* check current dir? */
#endif /* IGNORE_DOT_PATH */

    command[0] = '\0';

    if (strlen(file) >= MAXPATHLEN) {
	errno = ENAMETOOLONG;
	(void) fprintf(stderr, "%s:  path too long:  %s\n", Argv[0], file);
	exit(1);
    }

    /*
     * If we were given a fully qualified or relative path
     * there is no need to look at PATH.
     */
    if (strchr(file, '/')) {
	(void) strcpy(command, file);
	return(sudo_goodpath(command));
    }

    /*
     * grab PATH out of environment and make a local copy
     */
    if ((path = getenv("PATH")) == NULL)
	return(NULL);

    if ((path = (char *) strdup(path)) == NULL) {
	(void) fprintf(stderr, "%s: out of memory!\n", Argv[0]);
	exit(1);
    }
    origpath=path;

    /* XXX use strtok() */
    do {
	if ((n = strchr(path, ':')))
	    *n = '\0';

	/*
	 * search current dir last if it is in PATH This will miss sneaky
	 * things like using './' or './/' 
	 */
	if (*path == '\0' || (*path == '.' && *(path + 1) == '\0')) {
#ifndef IGNORE_DOT_PATH
	    checkdot = 1;
#endif /* IGNORE_DOT_PATH */
	    path = n + 1;
	    continue;
	}

	/*
	 * resolve the path and exit the loop if found
	 */
	if (strlen(path) + strlen(file) + 1 >= MAXPATHLEN) {
	    (void) fprintf(stderr, "%s:  path too long:  %s\n", Argv[0], file);
	    exit(1);
	}
	(void) sprintf(command, "%s/%s", path, file);
	if ((result = sudo_goodpath(command)))
	    break;

	path = n + 1;

    } while (n);

#ifndef IGNORE_DOT_PATH
    /*
     * check current dir if dot was in the PATH
     */
    if (!result && checkdot)
	result = sudo_goodpath(file);
#endif /* IGNORE_DOT_PATH */

    (void) free(origpath);

    return(result);
}
