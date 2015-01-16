/*	$OpenBSD: failedlogin.c,v 1.17 2015/01/16 06:40:09 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * failedlogin.c
 *	Log to failedlogin file and read from it, reporting the number of
 *	failed logins since the last good login and when/from where
 *	the last failed login was.
 */

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

void	 log_failedlogin(uid_t, char *, char *, char *);
int	 check_failedlogin(uid_t);

/*
 * Log a bad login to the failedlogin file.
 */
void
log_failedlogin(uid_t uid, char *host, char *name, char *tty)
{
	struct badlogin failedlogin;
	int fd;

	/* Add O_CREAT if you want to create failedlogin if it doesn't exist */
	if ((fd = open(_PATH_FAILEDLOGIN, O_RDWR, S_IRUSR|S_IWUSR)) >= 0) {
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
check_failedlogin(uid_t uid)
{
	struct badlogin failedlogin;
	int fd, was_bad = 0;

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
