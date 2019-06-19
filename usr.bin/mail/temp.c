/*	$OpenBSD: temp.c,v 1.18 2018/09/16 02:38:57 millert Exp $	*/
/*	$NetBSD: temp.c,v 1.5 1996/06/08 19:48:42 christos Exp $	*/

/*
 * Copyright (c) 1980, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "rcv.h"
#include <pwd.h>
#include "extern.h"

/*
 * Mail -- a mail program
 *
 * Give names to all the temporary files that we will need.
 */

char *tmpdir;

void
tinit(void)
{
	char *cp;

	tmpdir = _PATH_TMP;
	if ((tmpdir = strdup(tmpdir)) == NULL)
		err(1, "strdup");

	/* Strip trailing '/' if necessary */
	cp = tmpdir + strlen(tmpdir) - 1;
	while (cp > tmpdir && *cp == '/') {
		*cp = '\0';
		cp--;
	}

	/*
	 * It's okay to call savestr in here because main will
	 * do a spreserve() after us.
	 */
	if (myname != NULL) {
		uid_t uid;

		if (uid_from_user(myname, &uid) == -1)
			errx(1, "\"%s\" is not a user of this system", myname);
	} else {
		if ((myname = username()) == NULL) {
			myname = "nobody";
			if (rcvmode)
				exit(1);
		} else
			myname = savestr(myname);
	}
	if ((cp = getenv("HOME")) == NULL || *cp == '\0' ||
	    strlen(cp) >= PATHSIZE)
		homedir = NULL;
	else
		homedir = savestr(cp);
	if (debug)
		printf("user = %s, homedir = %s\n", myname,
		    homedir ? homedir : "NONE");
}
