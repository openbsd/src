/*	$OpenBSD: mail.local.c,v 1.19 1998/08/15 21:04:34 millert Exp $	*/

/*-
 * Copyright (c) 1996-1998 Theo de Raadt <deraadt@theos.com>
 * Copyright (c) 1996-1998 David Mazieres <dm@lcs.mit.edu>
 * Copyright (c) 1990 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1990 The Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)mail.local.c	5.6 (Berkeley) 6/19/91";
#else
static char rcsid[] = "$OpenBSD: mail.local.c,v 1.19 1998/08/15 21:04:34 millert Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/signal.h>
#include <syslog.h>
#include <fcntl.h>
#include <netdb.h>
#include <pwd.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pathnames.h"
#include "mail.local.h"

int
main(argc, argv)
	int argc;
	char **argv;
{
	struct passwd *pw;
	int ch, fd, eval, lockfile=1, holdme=0;
	uid_t uid;
	char *from;

	openlog("mail.local", LOG_PERROR, LOG_MAIL);

	from = NULL;
	while ((ch = getopt(argc, argv, "lLdf:r:H")) != -1)
		switch(ch) {
		case 'd':		/* backward compatible */
			break;
		case 'f':
		case 'r':		/* backward compatible */
			if (from)
			    err(FATAL, "multiple -f options");
			from = optarg;
			break;
		case 'l':
			lockfile=1;
			break;
		case 'L':
			lockfile=0;
			break;
		case 'H':
			holdme=1;
			break;
		case '?':
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	/* Support -H flag for backwards compat */
	if (holdme) {
		execl(_PATH_LOCKSPOOL, "lockspool", NULL);
		err(FATAL, "execl: lockspool: %s", strerror(errno));
	} else {
		if (!*argv)
			usage();
		if (geteuid() != 0)
			err(FATAL, "may only be run by the superuser");
	}

	/*
	 * If from not specified, use the name from getlogin() if the
	 * uid matches, otherwise, use the name from the password file
	 * corresponding to the uid.
	 */
	uid = getuid();
	if (!from && (!(from = getlogin()) ||
	    !(pw = getpwnam(from)) || pw->pw_uid != uid))
		from = (pw = getpwuid(uid)) ? pw->pw_name : "???";

	fd = store(from);
	for (eval = 0; *argv; ++argv)
		eval |= deliver(fd, *argv, lockfile);
	exit(eval);
}

int
store(from)
	char *from;
{
	FILE *fp;
	time_t tval;
	int fd, eline;
	char *tn, line[2048];

	if ((tn = strdup(_PATH_LOCTMP)) == NULL)
		err(FATAL, "unable to allocate memory");
	if ((fd = mkstemp(tn)) == -1 || !(fp = fdopen(fd, "w+")))
		err(FATAL, "unable to open temporary file");
	(void)unlink(tn);
	free(tn);

	(void)time(&tval);
	(void)fprintf(fp, "From %s %s", from, ctime(&tval));

	line[0] = '\0';
	for (eline = 1; fgets(line, sizeof(line), stdin);) {
		if (line[0] == '\n')
			eline = 1;
		else {
			if (eline && line[0] == 'F' && !bcmp(line, "From ", 5))
				(void)putc('>', fp);
			eline = 0;
		}
		(void)fprintf(fp, "%s", line);
		if (ferror(fp))
			break;
	}

	/* If message not newline terminated, need an extra. */
	if (!strchr(line, '\n'))
		(void)putc('\n', fp);
	/* Output a newline; note, empty messages are allowed. */
	(void)putc('\n', fp);

	(void)fflush(fp);
	if (ferror(fp))
		err(FATAL, "temporary file write error");
	return(fd);
}

int
deliver(fd, name, lockfile)
	int fd;
	char *name;
	int lockfile;
{
	struct stat sb, fsb;
	struct passwd *pw;
	int mbfd=-1, nr, nw, off, rval=1, lfd=-1;
	char biffmsg[100], buf[8*1024], path[MAXPATHLEN];
	off_t curoff;

	/*
	 * Disallow delivery to unknown names -- special mailboxes can be
	 * handled in the sendmail aliases file.
	 */
	if (!(pw = getpwnam(name))) {
		err(NOTFATAL, "unknown name: %s", name);
		return(1);
	}

	(void)snprintf(path, sizeof path, "%s/%s", _PATH_MAILDIR, name);

	if (lockfile) {
		lfd = getlock(name, pw);
		if (lfd == -1)
			return (1);
	}

	/* after this point, always exit via bad to remove lockfile */
retry:
	if (lstat(path, &sb)) {
		if (errno != ENOENT) {
			err(NOTFATAL, "%s: %s", path, strerror(errno));
			goto bad;
		}
		if ((mbfd = open(path, O_APPEND|O_CREAT|O_EXCL|O_WRONLY|O_EXLOCK,
		     S_IRUSR|S_IWUSR)) < 0) {
			if (errno == EEXIST) {
				/* file appeared since lstat */
				goto retry;
			} else {
				err(NOTFATAL, "%s: %s", path, strerror(errno));
				goto bad;
			}
		}
		/*
		 * Set the owner and group.  Historically, binmail repeated
		 * this at each mail delivery.  We no longer do this, assuming
		 * that if the ownership or permissions were changed there
		 * was a reason for doing so.
		 */
		if (fchown(mbfd, pw->pw_uid, pw->pw_gid) < 0) {
			err(NOTFATAL, "chown %u:%u: %s",
			    pw->pw_uid, pw->pw_gid, name);
			goto bad;
		}
	} else {
		if (sb.st_nlink != 1 || !S_ISREG(sb.st_mode)) {
			err(NOTFATAL, "%s: linked or special file", path);
			goto bad;
		}
		if ((mbfd = open(path, O_APPEND|O_WRONLY|O_EXLOCK,
		    S_IRUSR|S_IWUSR)) < 0) {
			err(NOTFATAL, "%s: %s", path, strerror(errno));
			goto bad;
		}
		if (fstat(mbfd, &fsb)) {
			/* relating error to path may be bad style */
			err(NOTFATAL, "%s: %s", path, strerror(errno));
			goto bad;
		}
		if (sb.st_dev != fsb.st_dev || sb.st_ino != fsb.st_ino) {
			err(NOTFATAL, "%s: changed after open", path);
			goto bad;
		}
		/* paranoia? */
		if (fsb.st_nlink != 1 || !S_ISREG(fsb.st_mode)) {
			err(NOTFATAL, "%s: linked or special file", path);
			goto bad;
		}
	}

	curoff = lseek(mbfd, 0, SEEK_END);
	(void)snprintf(biffmsg, sizeof biffmsg, "%s@%qd\n", name, curoff);
	if (lseek(fd, 0, SEEK_SET) == (off_t)-1) {
		err(FATAL, "temporary file: %s", strerror(errno));
		goto bad;
	}

	while ((nr = read(fd, buf, sizeof(buf))) > 0)
		for (off = 0; off < nr;  off += nw)
			if ((nw = write(mbfd, buf + off, nr - off)) < 0) {
				err(NOTFATAL, "%s: %s", path, strerror(errno));
				(void)ftruncate(mbfd, curoff);
				goto bad;
			}

	if (nr == 0) {
		rval = 0;
	} else {
		err(FATAL, "temporary file: %s", strerror(errno));
		(void)ftruncate(mbfd, curoff);
	}

bad:
	if (lfd != -1) {
		rellock();
		close(lfd);
	}

	if (mbfd != -1) {
		(void)fsync(mbfd);		/* Don't wait for update. */
		(void)close(mbfd);		/* Implicit unlock. */
	}

	if (!rval)
		notifybiff(biffmsg);
	return(rval);
}

void
notifybiff(msg)
	char *msg;
{
	static struct sockaddr_in addr;
	static int f = -1;
	struct hostent *hp;
	struct servent *sp;
	int len;

	if (!addr.sin_family) {
		/* Be silent if biff service not available. */
		if (!(sp = getservbyname("biff", "udp")))
			return;
		if (!(hp = gethostbyname("localhost"))) {
			err(NOTFATAL, "localhost: %s", strerror(errno));
			return;
		}
		addr.sin_len = sizeof(struct sockaddr_in);
		addr.sin_family = hp->h_addrtype;
		addr.sin_port = sp->s_port;
		bcopy(hp->h_addr, &addr.sin_addr, hp->h_length);
	}
	if (f < 0 && (f = socket(AF_INET, SOCK_DGRAM, 0)) == -1) {
		err(NOTFATAL, "socket: %s", strerror(errno));
		return;
	}
	len = strlen(msg) + 1;
	if (sendto(f, msg, len, 0, (struct sockaddr *)&addr, sizeof(addr))
	    != len)
		err(NOTFATAL, "sendto biff: %s", strerror(errno));
}

void
usage()
{
	err(FATAL, "usage: mail.local [-lL] [-f from] user ...");
}
