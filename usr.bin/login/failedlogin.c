/*	$OpenBSD: failedlogin.c,v 1.9 2000/03/02 04:05:44 millert Exp $	*/

/*
 * Copyright (c) 1996 Todd C. Miller <Todd.Miller@courtesan.com>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#ifndef lint                                                              
static char rcsid[] = "$OpenBSD: failedlogin.c,v 1.9 2000/03/02 04:05:44 millert Exp $";
#endif /* not lint */                                                        

/*
 * failedlogin.c
 *	Log to failedlogin file and read from it, reporting the number of
 *	failed logins since the last good login and when/from where
 *	the last failed login was.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <utmp.h>

#include "pathnames.h"

struct badlogin {
	char	bl_line[UT_LINESIZE];	/* tty used */
	char	bl_name[UT_NAMESIZE];	/* remote username */
	char	bl_host[UT_HOSTSIZE];	/* remote host */
	time_t	bl_time;		/* time of the login attempt */
	size_t	count;			/* number of bad logins */
};

/*
 * Log a bad login to the failedlogin file.
 */
void
log_failedlogin(uid, host, name, tty)
	uid_t uid;
	char *host, *name, *tty;
{
	struct badlogin failedlogin;
	int fd;

	/* Add O_CREAT if you want to create failedlogin if it doesn't exist */
	if ((fd = open(_PATH_FAILEDLOGIN, O_RDWR, S_IREAD|S_IWRITE)) >= 0) {
		(void)lseek(fd, (off_t)uid * sizeof(failedlogin), SEEK_SET);

		/* Read in last bad login so can get the count */
		if (read(fd, (char *)&failedlogin, sizeof(failedlogin)) !=
			sizeof(failedlogin) || failedlogin.bl_time == 0)
			memset((void *)&failedlogin, 0, sizeof(failedlogin));

		(void)lseek(fd, (off_t)uid * sizeof(failedlogin), SEEK_SET);
		/* Increment count of bad logins */
		++failedlogin.count;
		(void)time(&failedlogin.bl_time);
		strncpy(failedlogin.bl_line, tty, sizeof(failedlogin.bl_line));
		if (host)
			strncpy(failedlogin.bl_host, host, sizeof(failedlogin.bl_host));
		else
			*failedlogin.bl_host = '\0';	/* NULL host field */
		if (name)
			strncpy(failedlogin.bl_name, name, sizeof(failedlogin.bl_name));
		else
			*failedlogin.bl_name = '\0';	/* NULL name field */
		(void)write(fd, (char *)&failedlogin, sizeof(failedlogin));
		(void)close(fd);
	}
}

/*
 * Check the failedlogin file and report about the number of unsuccessful
 * logins and info about the last one in lastlogin style.
 * NOTE: zeros the count field since this is assumed to be called after the
 * user has been validated.
 */
int
check_failedlogin(uid)
	uid_t uid;
{
	int fd;
	struct badlogin failedlogin;
	int was_bad = 0;

	(void)memset((void *)&failedlogin, 0, sizeof(failedlogin));

	if ((fd = open(_PATH_FAILEDLOGIN, O_RDWR, 0)) >= 0) {
		(void)lseek(fd, (off_t)uid * sizeof(failedlogin), SEEK_SET);
		if (read(fd, (char *)&failedlogin, sizeof(failedlogin)) ==
		    sizeof(failedlogin) && failedlogin.count > 0 ) {
			/* There was a bad login */
			was_bad = 1;
			if (failedlogin.count > 1)
				(void)printf("There have been %lu unsuccessful "
				    "login attempts to your account.\n",
				    (u_long)failedlogin.count);
			(void)printf("Last unsuccessful login: %.*s", 24-5,
				(char *)ctime(&failedlogin.bl_time));
			(void)printf(" on %.*s",
			    (int)sizeof(failedlogin.bl_line),
			    failedlogin.bl_line); 
			if (*failedlogin.bl_host != '\0') {
				if (*failedlogin.bl_name != '\0')
					(void)printf(" from %.*s@%.*s",
					    (int)sizeof(failedlogin.bl_name),
					    failedlogin.bl_name,
					    (int)sizeof(failedlogin.bl_host),
					    failedlogin.bl_host);
				else
					(void)printf(" from %.*s",
					    (int)sizeof(failedlogin.bl_host),
					    failedlogin.bl_host);
			}
			(void)putchar('\n');

			/* Reset since this is a good login and write record */
			failedlogin.count = 0;
			(void)lseek(fd, (off_t)uid * sizeof(failedlogin),
			    SEEK_SET);
			(void)write(fd, (char *)&failedlogin,
			    sizeof(failedlogin));
		}
		(void)close(fd);
	}
	return(was_bad);
}
