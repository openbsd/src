/*	$OpenBSD: user_backend.c,v 1.1 2011/05/17 18:54:32 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <event.h>
#include <imsg.h>
#include <libgen.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

int user_getpw_ret(struct user *, struct passwd *); /* helper */
int user_getpwnam(struct user *, char *);
int user_getpwuid(struct user *, uid_t);
struct user_backend *user_backend_lookup(enum user_type);

struct user_backend user_backends[] = {
	{ USER_GETPWNAM, user_getpwnam, user_getpwuid }
};

struct user_backend *
user_backend_lookup(enum user_type type)
{
	u_int8_t i;

	for (i = 0; i < nitems(user_backends); ++i)
		if (user_backends[i].type == type)
			break;

	if (i == nitems(user_backends))
		fatalx("invalid user type");

	return &user_backends[i];
}



int
user_getpw_ret(struct user *u, struct passwd *pw)
{
	if (strlcpy(u->username, pw->pw_name, sizeof (u->username))
	    >= sizeof (u->username))
		return 0;

	if (strlcpy(u->password, pw->pw_passwd, sizeof (u->password))
	    >= sizeof (u->password))
		return 0;

	if (strlcpy(u->directory, pw->pw_dir, sizeof (u->directory))
	    >= sizeof (u->directory))
		return 0;

	u->uid = pw->pw_uid;
	u->gid = pw->pw_gid;

	return 1;
}

int
user_getpwnam(struct user *u, char *username)
{
	struct passwd *pw;

	pw = getpwnam(username);
	if (pw == NULL)
		return 0;

	return user_getpw_ret(u, pw);
}

int
user_getpwuid(struct user *u, uid_t uid)
{
	struct passwd *pw;

	pw = getpwuid(uid);
	if (pw == NULL)
		return 0;

	return user_getpw_ret(u, pw);
}
