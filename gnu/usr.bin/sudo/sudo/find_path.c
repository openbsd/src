/*	$OpenBSD: find_path.c,v 1.10 1999/03/29 20:29:02 millert Exp $	*/

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
 *  This module contains the find_path() function that returns
 *  TRUE if the command was found and FALSE if not.
 *  If find_path() returns TRUE, the copyin paramters command and
 *  ocommand contain the resolved and unresolved pathnames respectively.
 *  NOTE: if "." or "" exists in PATH it will be searched last.
 *
 *  Todd C. Miller <Todd.Miller@courtesan.com> Sat Mar 25 21:50:36 MST 1995
 */

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
#include <errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include "sudo.h"

#ifndef STDC_HEADERS
extern char *getenv	__P((const char *));
extern char *strcpy	__P((char *, const char *));
extern int fprintf	__P((FILE *, const char *, ...));
extern ssize_t readlink	__P((const char *, VOID *, size_t));
extern int stat		__P((const char *, struct stat *));
extern int lstat	__P((const char *, struct stat *));
#endif /* !STDC_HEADERS */

#ifndef _S_IFMT
#define _S_IFMT		S_IFMT
#endif /* _S_IFMT */
#ifndef _S_IFLNK
#define _S_IFLNK	S_IFLNK
#endif /* _S_IFLNK */

#ifndef lint
static const char rcsid[] = "$Sudo: find_path.c,v 1.85 1999/03/29 04:05:08 millert Exp $";
#endif /* lint */

/*******************************************************************
 *
 *  find_path()
 *
 *  this function finds the full pathname for a command and
 *  stores it in a statically allocated array, filling in a pointer
 *  to the array.  Returns FOUND if the command was found, NOT_FOUND
 *  if it was not found, or NOT_FOUND_DOT if it would have been found
 *  but it is in '.' and IGNORE_DOT_PATH is in effect.
 */

int find_path(infile, outfile)
    char *infile;		/* file to find */
    char **outfile;		/* result parameter */
{
    static char command[MAXPATHLEN]; /* qualified filename */
    register char *n;		/* for traversing path */
    char *path = NULL;		/* contents of PATH env var */
    char *origpath;		/* so we can free path later */
    char *result = NULL;	/* result of path/file lookup */
    int checkdot = 0;		/* check current dir? */

    command[0] = '\0';

    if (strlen(infile) >= MAXPATHLEN) {
	errno = ENAMETOOLONG;
	(void) fprintf(stderr, "%s: path too long: %s\n", Argv[0], infile);
	exit(1);
    }

    /*
     * If we were given a fully qualified or relative path
     * there is no need to look at PATH.
     */
    if (strchr(infile, '/')) {
	(void) strcpy(command, infile);
	if (sudo_goodpath(command)) {
	    *outfile = command;
	    return(FOUND);
	} else
	    return(NOT_FOUND);
    }

    /*
     * grab PATH out of environment and make a local copy
     */
    if ((path = getenv("PATH")) == NULL)
	return(NOT_FOUND);

    path = estrdup(path);
    origpath = path;

    /* XXX use strtok() */
    do {
	if ((n = strchr(path, ':')))
	    *n = '\0';

	/*
	 * search current dir last if it is in PATH This will miss sneaky
	 * things like using './' or './/' 
	 */
	if (*path == '\0' || (*path == '.' && *(path + 1) == '\0')) {
	    checkdot = 1;
	    path = n + 1;
	    continue;
	}

	/*
	 * resolve the path and exit the loop if found
	 */
	if (strlen(path) + strlen(infile) + 1 >= MAXPATHLEN) {
	    (void) fprintf(stderr, "%s: path too long: %s\n", Argv[0], infile);
	    exit(1);
	}
	(void) sprintf(command, "%s/%s", path, infile);
	if ((result = sudo_goodpath(command)))
	    break;

	path = n + 1;

    } while (n);
    (void) free(origpath);

    /*
     * Check current dir if dot was in the PATH
     */
    if (!result && checkdot) {
	result = sudo_goodpath(infile);
#ifdef IGNORE_DOT_PATH
	if (result)
	    return(NOT_FOUND_DOT);
#endif /* IGNORE_DOT_PATH */
    }

    if (result) {
	*outfile = result;
	return(FOUND);
    } else
	return(NOT_FOUND);
}
