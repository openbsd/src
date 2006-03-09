/*	$OpenBSD: login_passwd.c,v 1.9 2006/03/09 19:14:10 millert Exp $	*/

/*-
 * Copyright (c) 2001 Hans Insulander <hin@openbsd.org>.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "common.h"

int
pwd_login(char *username, char *password, char *wheel, int lastchance,
    char *class)
{
	struct passwd *pwd;
	login_cap_t *lc;
	size_t plen;
	char *salt, saltbuf[_PASSWORD_LEN + 1];

	if (wheel != NULL && strcmp(wheel, "yes") != 0) {
		fprintf(back, BI_VALUE " errormsg %s\n",
		    auth_mkvalue("you are not in group wheel"));
		fprintf(back, BI_REJECT "\n");
		return (AUTH_FAILED);
	}
	if (password == NULL)
		return (AUTH_FAILED);

	pwd = getpwnam(username);
	if (pwd)
		salt = pwd->pw_passwd;
	else {
		/* no such user, get appropriate salt */
		if ((lc = login_getclass(NULL)) == NULL ||
		    pwd_gensalt(saltbuf, sizeof(saltbuf), lc, 'l') == 0)
			salt = "xx";
		else
			salt = saltbuf;
	}

	setpriority(PRIO_PROCESS, 0, -4);

	salt = crypt(password, salt);
	plen = strlen(password);
	memset(password, 0, plen);

	/*
	 * Authentication fails if the user does not exist in the password
	 * database, the given password does not match the entry in the
	 * password database, or if the user's password field is empty
	 * and the given password is not the empty string.
	 */
	if (!pwd || strcmp(salt, pwd->pw_passwd) != 0 ||
	    (*pwd->pw_passwd == '\0' && plen > 0))
		return (AUTH_FAILED);

	if (login_check_expire(back, pwd, class, lastchance) == 0)
		fprintf(back, BI_AUTH "\n");
	else
		return (AUTH_FAILED);

	return (AUTH_OK);
}
