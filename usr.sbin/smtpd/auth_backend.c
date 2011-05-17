/*	$OpenBSD: auth_backend.c,v 1.1 2011/05/17 16:42:06 gilles Exp $	*/

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

#include <bsd_auth.h>
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

int auth_bsd(char *, char *);
int auth_getpwnam(char *, char *);
struct auth_backend *auth_backend_lookup(enum auth_type);

struct auth_backend auth_backends[] = {
	{ AUTH_BSD,		auth_bsd	},
	{ AUTH_GETPWNAM,	auth_getpwnam	}
};

struct auth_backend *
auth_backend_lookup(enum auth_type type)
{
	u_int8_t i;

	for (i = 0; i < nitems(auth_backends); ++i)
		if (auth_backends[i].type == type)
			break;

	if (i == nitems(auth_backends))
		fatalx("invalid auth type");

	return &auth_backends[i];
}


int
auth_bsd(char *username, char *password)
{
	return auth_userokay(username, NULL, "auth-smtp", password);
}


int
auth_getpwnam(char *username, char *password)
{
	struct passwd *pw;

	pw = getpwnam(username);
	if (pw == NULL)
		return 0;

	if (strcmp(pw->pw_passwd, crypt(password, pw->pw_passwd)) == 0)
		return 1;

	return 0;
}
