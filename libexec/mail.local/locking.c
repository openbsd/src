/*	$OpenBSD: locking.c,v 1.2 1998/08/15 23:11:30 millert Exp $	*/

/*
 * Copyright (c) 1996-1998 Theo de Raadt <deraadt@theos.com>
 * Copyright (c) 1996-1998 David Mazieres <dm@lcs.mit.edu>
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
 * 3. The name of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef lint
static char rcsid[] = "$OpenBSD: locking.c,v 1.2 1998/08/15 23:11:30 millert Exp $";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pwd.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "pathnames.h"
#include "mail.local.h"

static char lpath[MAXPATHLEN];

void
rellock()
{

	if (lpath[0])
		unlink(lpath);
}

int
getlock(name, pw)
	char *name;
	struct passwd *pw;
{
	struct stat sb, fsb;
	int lfd=-1;
	char buf[8*1024];
	int tries = 0;

	(void)snprintf(lpath, sizeof lpath, "%s/%s.lock",
	    _PATH_MAILDIR, name);

	if (stat(_PATH_MAILDIR, &sb) != -1 &&
	    (sb.st_mode & S_IWOTH) == S_IWOTH) {
		/*
		 * We have a writeable spool, deal with it as
		 * securely as possible.
		 */
		time_t ctim = -1;

		seteuid(pw->pw_uid);
		if (lstat(lpath, &sb) != -1)
			ctim = sb.st_ctime;
		while (1) {
			/*
			 * Deal with existing user.lock files
			 * or directories or symbolic links that
			 * should not be here.
			 */
			if (readlink(lpath, buf, sizeof buf-1) != -1) {
				if (lstat(lpath, &sb) != -1 &&
				    S_ISLNK(fsb.st_mode)) {
					seteuid(sb.st_uid);
					unlink(lpath);
					seteuid(pw->pw_uid);
				}
				goto again;
			}
			if ((lfd = open(lpath, O_CREAT|O_WRONLY|O_EXCL|O_EXLOCK,
			    S_IRUSR|S_IWUSR)) != -1)
				break;
again:
			if (tries > 10) {
				err(NOTFATAL, "%s: %s", lpath,
				    strerror(errno));
				seteuid(0);
				return(-1);
			}
			if (tries > 9 &&
			    (lfd = open(lpath, O_WRONLY|O_EXLOCK, 0)) != -1) {
				if (fstat(lfd, &fsb) != -1 &&
				    lstat(lpath, &sb) != -1) {
					if (fsb.st_dev == sb.st_dev &&
					    fsb.st_ino == sb.st_ino &&
					    ctim == fsb.st_ctime ) {
						seteuid(fsb.st_uid);
						baditem(lpath);
						seteuid(pw->pw_uid);
					}
				}
			}
			sleep(1 << tries);
			tries++;
			continue;
		}
		seteuid(0);
	} else {
		/*
		 * Only root can write the spool directory.
		 */
		while (1) {
			if ((lfd = open(lpath, O_CREAT|O_WRONLY|O_EXCL,
			    S_IRUSR|S_IWUSR)) != -1)
				break;
			if (tries > 9) {
				err(NOTFATAL, "%s: %s", lpath, strerror(errno));
				return(-1);
			}
			sleep(1 << tries);
			tries++;
		}
	}
	return(lfd);
}

void
baditem(path)
	char *path;
{
	char npath[MAXPATHLEN];

	if (unlink(path) == 0)
		return;
	snprintf(npath, sizeof npath, "%s/mailXXXXXXXXXX", _PATH_MAILDIR);
	if (mktemp(npath) == NULL)
		return;
	if (rename(path, npath) == -1)
		unlink(npath);
	else
		err(NOTFATAL, "nasty spool item %s renamed to %s",
		    path, npath);
	/* XXX if we fail to rename, another attempt will happen later */
}

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

void
#ifdef __STDC__
err(int isfatal, const char *fmt, ...)
#else
err(isfatal, fmt)
	int isfatal;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	vsyslog(LOG_ERR, fmt, ap);
	va_end(ap);
	if (isfatal)
		exit(1);
}
