/*
 * Copyright (c) 1996, 1998, 1999 Todd C. Miller <Todd.Miller@courtesan.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * 4. Products derived from this software may not be called "Sudo" nor
 *    may "Sudo" appear in their names without specific prior written
 *    permission from the author.
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

#include "sudo.h"

#ifndef STDC_HEADERS
extern char *getenv	__P((const char *));
extern char *strcpy	__P((char *, const char *));
extern int fprintf	__P((FILE *, const char *, ...));
extern ssize_t readlink	__P((const char *, VOID *, size_t));
extern int stat		__P((const char *, struct stat *));
extern int lstat	__P((const char *, struct stat *));
#endif /* !STDC_HEADERS */

#ifndef lint
static const char rcsid[] = "$Sudo: find_path.c,v 1.95 2000/01/27 04:31:58 millert Exp $";
#endif /* lint */

/*
 * This function finds the full pathname for a command and
 * stores it in a statically allocated array, filling in a pointer
 * to the array.  Returns FOUND if the command was found, NOT_FOUND
 * if it was not found, or NOT_FOUND_DOT if it would have been found
 * but it is in '.' and IGNORE_DOT is set.
 */
int
find_path(infile, outfile)
    char *infile;		/* file to find */
    char **outfile;		/* result parameter */
{
    static char command[MAXPATHLEN]; /* qualified filename */
    char *n;			/* for traversing path */
    char *path = NULL;		/* contents of PATH env var */
    char *origpath;		/* so we can free path later */
    char *result = NULL;	/* result of path/file lookup */
    int checkdot = 0;		/* check current dir? */

    if (strlen(infile) >= MAXPATHLEN) {
	(void) fprintf(stderr, "%s: path too long: %s\n", Argv[0], infile);
	exit(1);
    }

    /*
     * If we were given a fully qualified or relative path
     * there is no need to look at $PATH.
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
     * Grab PATH out of the environment (or from the string table
     * if SECURE_PATH is in effect) and make a local copy.
     */
    if (def_str(I_SECURE_PATH) && !user_is_exempt())
	path = def_str(I_SECURE_PATH);
    else if ((path = getenv("PATH")) == NULL)
	return(NOT_FOUND);
    path = estrdup(path);
    origpath = path;

    do {
	if ((n = strchr(path, ':')))
	    *n = '\0';

	/*
	 * Search current dir last if it is in PATH This will miss sneaky
	 * things like using './' or './/' 
	 */
	if (*path == '\0' || (*path == '.' && *(path + 1) == '\0')) {
	    checkdot = 1;
	    path = n + 1;
	    continue;
	}

	/*
	 * Resolve the path and exit the loop if found.
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
    free(origpath);

    /*
     * Check current dir if dot was in the PATH
     */
    if (!result && checkdot) {
	result = sudo_goodpath(infile);
	if (result && def_flag(I_IGNORE_DOT))
	    return(NOT_FOUND_DOT);
    }

    if (result) {
	*outfile = result;
	return(FOUND);
    } else
	return(NOT_FOUND);
}
