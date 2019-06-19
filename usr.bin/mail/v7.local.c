/*	$OpenBSD: v7.local.c,v 1.18 2018/09/16 02:38:57 millert Exp $	*/
/*	$NetBSD: v7.local.c,v 1.8 1997/05/13 06:15:58 mikel Exp $	*/

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

/*
 * Mail -- a mail program
 *
 * Version 7
 *
 * Local routines that are installation dependent.
 */

#include "rcv.h"
#include <stdlib.h>
#include <fcntl.h>
#include <pwd.h>
#include "extern.h"

/*
 * Locate the user's mailbox file (ie, the place where new, unread
 * mail is queued).
 */
void
findmail(const char *user, char *buf, int buflen)
{
	char *mbox;
	struct stat sb;

	/* Ignore $MAIL if it is not owned by the invoking user */
	if ((mbox = getenv("MAIL")) && stat(mbox, &sb) == 0 &&
	    sb.st_uid != getuid() && sb.st_uid != geteuid())
		mbox = NULL;

	if (mbox)
		(void)strlcpy(buf, mbox, buflen);
	else
		(void)snprintf(buf, buflen, "%s/%s", _PATH_MAILDIR, user);
}

/*
 * Get rid of the queued mail.
 */
void
demail(void)
{
	int fd;

	if (value("keep") != NULL || rm(mailname) < 0) {
		fd = open(mailname, O_CREAT | O_TRUNC | O_WRONLY, 0600);
		if (fd != -1)
			close(fd);
	}
}

/*
 * Discover user login name.
 */
const char *
username(void)
{
	const char *np;
	uid_t uid;

	if ((np = getenv("USER")) != NULL)
		return(np);
	if ((np = getenv("LOGNAME")) != NULL)
		return(np);
	if ((np = user_from_uid(uid = getuid(), 1)) != NULL)
		return(np);
	if ((np = getlogin()) != NULL)
		return(np);
	printf("Cannot associate a name with uid %u\n", uid);
	return(NULL);
}
