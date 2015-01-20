/* $OpenBSD: sshlogin.c,v 1.31 2015/01/20 23:14:00 deraadt Exp $ */
/*
 * Author: Tatu Ylonen <ylo@cs.hut.fi>
 * Copyright (c) 1995 Tatu Ylonen <ylo@cs.hut.fi>, Espoo, Finland
 *                    All rights reserved
 * This file performs some of the things login(1) normally does.  We cannot
 * easily use something like login -p -h host -f user, because there are
 * several different logins around, and it is hard to determined what kind of
 * login the current system has.  Also, we want to be able to execute commands
 * on a tty.
 *
 * As far as I am concerned, the code I have written for this software
 * can be used freely for any purpose.  Any derived versions of this
 * software must be clearly marked as such, and if the derived work is
 * incompatible with the protocol description in the RFC file, it must be
 * called by a name other than "ssh" or "Secure Shell".
 *
 * Copyright (c) 1999 Theo de Raadt.  All rights reserved.
 * Copyright (c) 1999 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <util.h>
#include <utmp.h>
#include <stdarg.h>
#include <limits.h>

#include "sshlogin.h"
#include "log.h"
#include "buffer.h"
#include "misc.h"
#include "servconf.h"

extern Buffer loginmsg;
extern ServerOptions options;

/*
 * Returns the time when the user last logged in.  Returns 0 if the
 * information is not available.  This must be called before record_login.
 * The host the user logged in from will be returned in buf.
 */
time_t
get_last_login_time(uid_t uid, const char *logname,
    char *buf, size_t bufsize)
{
	struct lastlog ll;
	char *lastlog;
	int fd;
	off_t pos, r;

	lastlog = _PATH_LASTLOG;
	buf[0] = '\0';

	fd = open(lastlog, O_RDONLY);
	if (fd < 0)
		return 0;

	pos = (long) uid * sizeof(ll);
	r = lseek(fd, pos, SEEK_SET);
	if (r == -1) {
		error("%s: lseek: %s", __func__, strerror(errno));
		close(fd);
		return (0);
	}
	if (r != pos) {
		debug("%s: truncated lastlog", __func__);
		close(fd);
		return (0);
	}
	if (read(fd, &ll, sizeof(ll)) != sizeof(ll)) {
		close(fd);
		return 0;
	}
	close(fd);
	if (bufsize > sizeof(ll.ll_host) + 1)
		bufsize = sizeof(ll.ll_host) + 1;
	strncpy(buf, ll.ll_host, bufsize - 1);
	buf[bufsize - 1] = '\0';
	return (time_t)ll.ll_time;
}

/*
 * Generate and store last login message.  This must be done before
 * login_login() is called and lastlog is updated.
 */
static void
store_lastlog_message(const char *user, uid_t uid)
{
	char *time_string, hostname[HOST_NAME_MAX+1] = "", buf[512];
	time_t last_login_time;

	if (!options.print_lastlog)
		return;

	last_login_time = get_last_login_time(uid, user, hostname,
	    sizeof(hostname));

	if (last_login_time != 0) {
		time_string = ctime(&last_login_time);
		time_string[strcspn(time_string, "\n")] = '\0';
		if (strcmp(hostname, "") == 0)
			snprintf(buf, sizeof(buf), "Last login: %s\r\n",
			    time_string);
		else
			snprintf(buf, sizeof(buf), "Last login: %s from %s\r\n",
			    time_string, hostname);
		buffer_append(&loginmsg, buf, strlen(buf));
	}
}

/*
 * Records that the user has logged in.  I wish these parts of operating
 * systems were more standardized.
 */
void
record_login(pid_t pid, const char *tty, const char *user, uid_t uid,
    const char *host, struct sockaddr *addr, socklen_t addrlen)
{
	int fd;
	struct lastlog ll;
	char *lastlog;
	struct utmp u;

	/* save previous login details before writing new */
	store_lastlog_message(user, uid);

	/* Construct an utmp/wtmp entry. */
	memset(&u, 0, sizeof(u));
	strncpy(u.ut_line, tty + 5, sizeof(u.ut_line));
	u.ut_time = time(NULL);
	strncpy(u.ut_name, user, sizeof(u.ut_name));
	strncpy(u.ut_host, host, sizeof(u.ut_host));

	login(&u);
	lastlog = _PATH_LASTLOG;

	/* Update lastlog unless actually recording a logout. */
	if (strcmp(user, "") != 0) {
		/*
		 * It is safer to memset the lastlog structure first because
		 * some systems might have some extra fields in it (e.g. SGI)
		 */
		memset(&ll, 0, sizeof(ll));

		/* Update lastlog. */
		ll.ll_time = time(NULL);
		strncpy(ll.ll_line, tty + 5, sizeof(ll.ll_line));
		strncpy(ll.ll_host, host, sizeof(ll.ll_host));
		fd = open(lastlog, O_RDWR);
		if (fd >= 0) {
			lseek(fd, (off_t) ((long) uid * sizeof(ll)), SEEK_SET);
			if (write(fd, &ll, sizeof(ll)) != sizeof(ll))
				logit("Could not write %.100s: %.100s", lastlog, strerror(errno));
			close(fd);
		}
	}
}

/* Records that the user has logged out. */
void
record_logout(pid_t pid, const char *tty)
{
	const char *line = tty + 5;	/* /dev/ttyq8 -> ttyq8 */
	if (logout(line))
		logwtmp(line, "", "");
}
