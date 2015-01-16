/*	$OpenBSD: field.c,v 1.14 2015/01/16 06:40:06 deraadt Exp $	*/
/*	$NetBSD: field.c,v 1.3 1995/03/26 04:55:28 glass Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
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

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <paths.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "chpass.h"

/* ARGSUSED */
int
p_login(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!*p) {
		warnx("empty login field");
		return (1);
	}
	if (*p == '-') {
		warnx("login names may not begin with a hyphen");
		return (1);
	}
	/* XXX - what about truncated names? */
	if (strcmp(pw->pw_name, p) != 0 && getpwnam(p) != NULL) {
		warnx("login %s already exists", p);
		return (1);
	}
	if (!(pw->pw_name = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	if (strchr(p, '.'))
		warnx("\'.\' is dangerous in a login name");
	for (; *p; ++p)
		if (isupper((unsigned char)*p)) {
			warnx("upper-case letters are dangerous in a login name");
			break;
		}
	return (0);
}

/* ARGSUSED */
int
p_passwd(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!*p)
		pw->pw_passwd = "";	/* "NOLOGIN"; */
	else if (!(pw->pw_passwd = strdup(p))) {
		warnx("can't save password entry");
		return (1);
	}

	return (0);
}

/* ARGSUSED */
int
p_uid(char *p, struct passwd *pw, ENTRY *ep)
{
	uid_t id;
	const char *errstr;

	if (!*p) {
		warnx("empty uid field");
		return (1);
	}
	id = (uid_t)strtonum(p, 0, UID_MAX, &errstr);
	if (errstr) {
		warnx("uid is %s", errstr);
		return (1);
	}
	pw->pw_uid = id;
	return (0);
}

/* ARGSUSED */
int
p_gid(char *p, struct passwd *pw, ENTRY *ep)
{
	struct group *gr;
	const char *errstr;
	gid_t id;

	if (!*p) {
		warnx("empty gid field");
		return (1);
	}
	if (!isdigit((unsigned char)*p)) {
		if (!(gr = getgrnam(p))) {
			warnx("unknown group %s", p);
			return (1);
		}
		pw->pw_gid = gr->gr_gid;
		return (0);
	}
	id = (uid_t)strtonum(p, 0, GID_MAX, &errstr);
	if (errstr) {
		warnx("gid is %s", errstr);
		return (1);
	}
	pw->pw_gid = id;
	return (0);
}

/* ARGSUSED */
int
p_class(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!*p)
		pw->pw_class = "";
	else if (!(pw->pw_class = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}

	return (0);
}

/* ARGSUSED */
int
p_change(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!atot(p, &pw->pw_change))
		return (0);
	warnx("illegal date for change field");
	return (1);
}

/* ARGSUSED */
int
p_expire(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!atot(p, &pw->pw_expire))
		return (0);
	warnx("illegal date for expire field");
	return (1);
}

/* ARGSUSED */
int
p_gecos(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!*p)
		ep->save = "";
	else if (!(ep->save = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_hdir(char *p, struct passwd *pw, ENTRY *ep)
{
	if (!*p) {
		warnx("empty home directory field");
		return (1);
	}
	if (!(pw->pw_dir = strdup(p))) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}

/* ARGSUSED */
int
p_shell(char *p, struct passwd *pw, ENTRY *ep)
{
	char *t;

	if (!*p) {
		pw->pw_shell = _PATH_BSHELL;
		return (0);
	}
	/* only admin can change from or to "restricted" shells */
	if (uid && pw->pw_shell && !ok_shell(pw->pw_shell, NULL)) {
		warnx("%s: current shell non-standard", pw->pw_shell);
		return (1);
	}
	if (!ok_shell(p, &t)) {
		if (uid) {
			warnx("%s: non-standard shell", p);
			return (1);
		} else
			t = strdup(p);
	}
	if (!(pw->pw_shell = t)) {
		warnx("can't save entry");
		return (1);
	}
	return (0);
}
