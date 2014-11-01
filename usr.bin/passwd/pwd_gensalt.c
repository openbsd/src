/*	$OpenBSD: pwd_gensalt.c,v 1.29 2014/11/01 17:48:00 tedu Exp $ */

/*
 * Copyright 1997 Niels Provos <provos@physnet.uni-hamburg.de>
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
 *      This product includes software developed by Niels Provos.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <grp.h>
#include <pwd.h>
#include <time.h>
#include <login_cap.h>

void	to64(char *, u_int32_t, int n);
int	pwd_gensalt(char *, int, login_cap_t *, char);

#define	CIPHER_DEF		"blowfish,8"

int
pwd_gensalt(char *salt, int saltlen, login_cap_t *lc, char type)
{
	char *next, *now, *oldnext;

	*salt = '\0';

	next = login_getcapstr(lc, "localcipher", NULL, NULL);
	if (next == NULL && (next = strdup(CIPHER_DEF)) == NULL) {
		warn(NULL);
		return 0;
	}

	oldnext = next;
	now = strsep(&next, ",");
	if (!strcmp(now, "blowfish")) {
		int rounds = 8;

		if (next)
			rounds = atoi(next);
		if (rounds < 4)
			rounds = 4;
		if (rounds > 31)
			rounds = 31;
		strlcpy(salt, bcrypt_gensalt(rounds), saltlen);
	} else {
		warnx("Unknown option %s.", now);
		free(oldnext);
		return 0;
	}
	free(oldnext);
	return 1;
}

static unsigned char itoa64[] =	 /* 0 ... 63 => ascii - 64 */
	"./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

void
to64(char *s, u_int32_t v, int n)
{
	while (--n >= 0) {
		*s++ = itoa64[v&0x3f];
		v >>= 6;
	}
}
