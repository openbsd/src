/*	$OpenBSD: auth_pwd.c,v 1.1 2011/12/14 22:28:02 eric Exp $	*/

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
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"

int auth_pwd(char *, char *);

struct auth_backend	auth_backend_pwd = {
	auth_pwd
};

int
auth_pwd(char *username, char *password)
{
	struct passwd *pw;

	pw = getpwnam(username);
	if (pw == NULL)
		return 0;

	if (strcmp(pw->pw_passwd, crypt(password, pw->pw_passwd)) == 0)
		return 1;

	return 0;
}
