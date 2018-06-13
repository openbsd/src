/*	$OpenBSD: login_passwd.c,v 1.15 2018/06/13 15:02:09 reyk Exp $	*/

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
    char *class, struct passwd *pwd)
{
	size_t plen;
	char *goodhash = NULL;
	int passok = 0;

	if (wheel != NULL && strcmp(wheel, "yes") != 0) {
		fprintf(back, BI_VALUE " errormsg %s\n",
		    auth_mkvalue("you are not in group wheel"));
		fprintf(back, BI_REJECT "\n");
		return (AUTH_FAILED);
	}
	if (password == NULL)
		return (AUTH_FAILED);

	if (pwd)
		goodhash = pwd->pw_passwd;

	setpriority(PRIO_PROCESS, 0, -4);

	if (pledge("stdio rpath", NULL) == -1) {
		syslog(LOG_ERR, "pledge: %m");
		return (AUTH_FAILED);
	}

	if (crypt_checkpass(password, goodhash) == 0)
		passok = 1;
	plen = strlen(password);
	explicit_bzero(password, plen);

	if (!passok)
		return (AUTH_FAILED);

	if (login_check_expire(back, pwd, class, lastchance) == 0)
		fprintf(back, BI_AUTH "\n");
	else
		return (AUTH_FAILED);

	return (AUTH_OK);
}
